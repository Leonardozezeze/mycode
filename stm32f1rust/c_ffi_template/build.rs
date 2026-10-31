//! 构建脚本：
//!
//! 1. 调用 CMake 把 `c_src/`（CubeMX 生成的 C/C++ 代码）编译成静态库
//!    `libMXTemplate.a`——CMake 只编译、不链接；
//! 2. 把静态库、启动文件、链接脚本和链接参数交给 rustc，由 rustc 完成
//!    最终固件链接。
//!
//! 所以 `cargo build` 一条命令就能产出完整固件。

use std::env;
use std::path::{Path, PathBuf};
use std::process::Command;

/// CMake 工程目录（含 CMakeLists.txt / CMakePresets.json）
const C_SRC_DIR: &str = "c_src";
/// CMake 工程名，同时决定静态库文件名 lib<NAME>.a
const C_LIB_NAME: &str = "MXTemplate";

fn main() {
    let manifest_dir =
        PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR 未设置"));
    let c_src = manifest_dir.join(C_SRC_DIR);

    // 变化检测
    // 注意：不能监听整个 c_src，否则 CMake 输出目录 build/ 每次构建都会变化，
    // 导致 build.rs 无限重跑。这里只监听真正的源码和 CMake 脚本。
    for entry in [
        "Core",
        "Drivers",
        "cmake",
        "CMakeLists.txt",
        "CMakePresets.json",
        "startup_stm32f103xb.s",
        "STM32F103xx_FLASH.ld",
    ] {
        println!("cargo:rerun-if-changed={}", c_src.join(entry).display());
    }

    // 主机端 `cargo test-host` 只编译 Rust，不构建 C，也不输出 ARM 链接参数
    if env::var("TARGET").as_deref() != Ok("thumbv7m-none-eabi") {
        return;
    }

    // cargo 的 PROFILE 为 debug / release（自定义 profile 以 release/optimized 命名也走 Release）
    let preset = if env::var("PROFILE")
        .map(|p| p.contains("release") || p.contains("opt"))
        .unwrap_or(false)
    {
        "Release"
    } else {
        "Debug"
    };

    // 与 CMakePresets.json 中的 binaryDir 保持一致：
    // binaryDir = ${sourceDir}/build/${presetName}
    let build_dir = c_src.join("build").join(preset);

    // ---------- 1. CMake：配置 + 编译，只产出 .o 和 .a ----------
    run_cmake(&c_src, &["--preset", preset]);
    run_cmake(&c_src, &["--build", "--preset", preset]);

    let archive = build_dir.join(format!("lib{C_LIB_NAME}.a"));
    if !archive.is_file() {
        panic!(
            "CMake 构建完成但找不到静态库：{}\n\
             请确认 c_src/CMakePresets.json 中 preset `{}` 的 binaryDir 仍为 ${{sourceDir}}/build/${{presetName}}",
            archive.display(),
            preset
        );
    }

    // ---------- 2. 链接：C 静态库 + 启动文件 + 链接脚本 ----------
    // 注意：必须把 C 静态库整体（whole-archive）链接进来。
    // 原因是 startup_stm32f103xb.s 为每个中断都提供了 .weak 别名（指向
    // Default_Handler），而 stm32f1xx_it.c 里复写的是强符号。归档成员只有在
    // 被引用时才会被拉取，向量表的引用已经被弱定义满足，it.o 就不会被链接，
    // 结果所有中断都跳进 Default_Handler（SysTick 不计数，HAL_Delay 卡死）。
    // whole-archive 保证强定义覆盖弱定义；未用到的函数由 --gc-sections 丢弃。
    println!("cargo:rustc-link-search=native={}", build_dir.display());
    // c_src 的 CMake 里 TOOLCHAIN_LINK_LIBRARIES = m
    println!("cargo:rustc-link-lib=m");

    let ld_script = c_src.join("STM32F103xx_FLASH.ld");
    println!("cargo:rustc-link-arg=-T{}", ld_script.display());
    println!("cargo:rustc-link-arg=-mcpu=cortex-m3");
    println!("cargo:rustc-link-arg=-mthumb");
    println!("cargo:rustc-link-arg=--specs=nano.specs");
    println!("cargo:rustc-link-arg=--specs=nosys.specs");
    // rustc 对 no_std 目标会传 -nodefaultlibs，newlib 需要手动链回来：
    // 启动文件的 __libc_init_array、C 代码用到的 memcpy/printf 等都在 libc 里。
    // -u 强制链接 newlib-nano 默认裁掉的 printf 浮点支持。
    println!("cargo:rustc-link-arg=-u");
    println!("cargo:rustc-link-arg=_printf_float");
    // C 静态库与 libc 互相引用（libc 的 _write_r 需要 syscalls.c 的 _write），
    // 用 group 反复扫描 libc；C 库本身已经整体进入链接。
    println!("cargo:rustc-link-arg=-Wl,--whole-archive");
    println!("cargo:rustc-link-arg={}", archive.display());
    println!("cargo:rustc-link-arg=-Wl,--no-whole-archive");
    println!("cargo:rustc-link-arg=-Wl,--start-group");
    println!("cargo:rustc-link-arg=-lc");
    println!("cargo:rustc-link-arg=-lgcc");
    println!("cargo:rustc-link-arg=-Wl,--end-group");
    // 丢弃未使用的函数/数据，减小固件体积
    println!("cargo:rustc-link-arg=-Wl,--gc-sections");

    // 生成 map 文件，方便排查固件体积和符号
    let map = PathBuf::from(env::var_os("OUT_DIR").expect("OUT_DIR 未设置"))
        .join("c_ffi_template.map");
    println!("cargo:rustc-link-arg=-Wl,-Map={}", map.display());
}

/// 运行 cmake 命令；失败时把 cmake 的输出原样打印出来，便于定位错误
fn run_cmake(dir: &Path, args: &[&str]) {
    let output = Command::new("cmake")
        .current_dir(dir)
        .args(args)
        .output()
        .unwrap_or_else(|e| panic!("无法执行 `cmake {args:?}`：{e}"));

    if !output.status.success() {
        panic!(
            "`cmake {args:?}` 失败（{}）\n--- stdout ---\n{}\n--- stderr ---\n{}",
            output.status,
            String::from_utf8_lossy(&output.stdout),
            String::from_utf8_lossy(&output.stderr),
        );
    }
}
