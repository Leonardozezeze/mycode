# STM32F1 Rust 开发规范

## 1. 项目定位

**CubeMX 生成底层 C 代码，Rust 写上层业务逻辑。**

- **C 侧**：CubeMX 生成的启动文件、链接脚本、HAL 驱动、`main.c` 里的时钟/外设初始化。
- **Rust 侧**：应用逻辑、外设封装。通过 `extern "C"` 调用 HAL 函数。
- **构建**：CMake 只负责把 C 代码编译成 `.o` / 静态库；最终链接（启动文件 + 链接脚本 + Rust 目标文件 + C 静态库）由 `build.rs` 交给 rustc 完成。

一条命令出固件：

```bash
make            # 构建 + 生成 .hex / .bin + 打印 FLASH/RAM 占用
```

## 1.1 从模板生成新项目

生成脚本 `new-project.sh` 位于模板的**上层目录**（与 `c_ffi_template/` 同级）：

```bash
../new-project.sh MyBoard                  # 在 c_ffi_template/ 内执行 → 生成 ../MyBoard
../new-project.sh MyBoard /path/to/MyBoard # 指定目标目录
../new-project.sh                          # 交互式输入项目名
```

脚本默认使用同级的 `c_ffi_template/` 作为模板，可用 `TEMPLATE_DIR=/path/to/c_ffi_template` 覆盖。

它会：

- 复制模板，排除 `target/`、`c_src/build/`、`.git/`；
- 把 `c_src/MXTemplate.ioc` 重命名为 `c_src/<名字>.ioc`；
- 将 `MXTemplate`（CMake 工程名、.ioc 内部工程名）和 `c_ffi_template`（Cargo 包名、Makefile、build.rs、文档）统一替换为新名字。

约束：目标目录名必须与项目名一致（`.vscode/launch.json` 用 `${workspaceFolderBasename}` 定位 ELF），且不能放在模板目录内部。

生成后：

```bash
cd ../MyBoard
make
# 用 STM32CubeMX 打开 c_src/MyBoard.ioc 继续配置外设
```

## 2. 控制流

复位后由 C 侧启动文件执行，`main()` 完成 CubeMX 初始化，然后跳到 Rust：

```
Reset_Handler (startup_stm32f103xb.s)
  → SystemInit
  → __libc_init_array
  → main()                              [Core/Src/main.c]
      HAL_Init
      SystemClock_Config
      MX_GPIO_Init / MX_DMA_Init
      MX_USART1_UART_Init / MX_ADC1_Init
      rust_run()  ──────────────────┐   [USER CODE BEGIN 2]
                                    │
                                    ▼
                              rust_run()  -> !        [src/main.rs]
                                  app_init()
                                  app_run()           [src/app/app.rs]
                                      HAL_GPIO_TogglePin / HAL_Delay ...
```

要点：

- **入口仍是 C 的 `main()`**（启动文件的向量表指向它），Rust 不提供 `main`，crate 使用 `#![no_main]`。
- `rust_run()` 标记为 `-> !`，进入后永不返回，C 侧后面的 `while (1)` 实际不会执行。
- Rust 调用 C 的方式是普通的 `unsafe extern "C" { fn HAL_xxx(...); }`，见 `src/hal/`。

## 3. 目录结构

```
c_ffi_template/
├── src/
│   ├── main.rs               # rust_run()：C 侧调用的唯一入口
│   ├── tests.rs              # 主机端单元测试（#[cfg(test)]，cargo test-host）
│   ├── app/
│   │   └── app.rs            # 应用初始化与主循环
│   ├── peripheral/           # 外设封装（安全 API）
│   │   └── led.rs
│   └── hal/                  # FFI 绑定
│       ├── gpio.rs
│       └── delay.rs
├── c_src/                    # CubeMX 生成 / C 代码
│   ├── CMakeLists.txt        # 只生成静态库 libMXTemplate.a
│   ├── CMakePresets.json     # Debug / Release 两个 preset
│   ├── Core/
│   │   ├── Src/main.c        # 初始化完成后调用 rust_run()
│   │   └── Inc/main.h        # USER CODE EFP 中声明 rust_run()
│   ├── Drivers/              # HAL + CMSIS
│   ├── cmake/
│   │   ├── gcc-arm-none-eabi.cmake
│   │   └── stm32cubemx/CMakeLists.txt
│   ├── startup_stm32f103xb.s
│   └── STM32F103xx_FLASH.ld
├── build.rs                  # 调 CMake 编译 C，再让 rustc 完成最终链接
├── Makefile                  # 一键构建并生成 .hex / .bin
├── scripts/
│   └── size-report.sh        # 打印 FLASH / RAM 占用
├── .cargo/config.toml        # 目标芯片、链接器、probe-rs runner
├── Cargo.toml
└── STM32F103.svd
```

> `c_src/App/` 已移除（原 C++ 上层代码）。如果以后仍想放一些 C/C++ 辅助代码，新建 `c_src/App/` 并配好 `App/CMakeLists.txt`，顶层 CMake 会自动加入构建（目录不存在时跳过）。

## 4. 构建

### 4.1 常规构建

```bash
make            # Debug：cargo build + 生成 .hex / .bin
make release    # Release：cargo build --release + 生成 .hex / .bin
make flash      # Release 构建并烧录（probe-rs）
make clean      # 清理 cargo 和 CMake 产物
```

只想要 ELF 时也可以直接用 cargo：

```bash
cargo build
cargo build --release  # CMake 用 Release preset，-Os
```

`build.rs` 会自动执行：

```bash
cmake --preset Debug --build ...     # 等价于在 c_src/ 下配置 + 编译
```

不需要手动进 `c_src/` 跑 cmake。

> Cargo 没有 post-build 钩子（`build.rs` 在链接之前运行），所以 `.hex` / `.bin` 由 [Makefile](Makefile) 在 `cargo build` 之后调用 `arm-none-eabi-objcopy` 生成。VSCode 的构建任务也已改为 `make`，按 `Ctrl+Shift+B` 或 F5 前的 preLaunchTask 都会一并生成。

### 4.2 产物位置

| 产物 | 路径 |
| --- | --- |
| C 目标文件 `.obj` | `c_src/build/<Debug\|Release>/**/*.obj` |
| C 静态库 | `c_src/build/<Debug\|Release>/libMXTemplate.a` |
| 最终固件 (ELF) | `target/thumbv7m-none-eabi/<debug\|release>/c_ffi_template` |
| Intel HEX | `target/thumbv7m-none-eabi/<debug\|release>/c_ffi_template.hex`（`make` 生成） |
| 原始二进制 | `target/thumbv7m-none-eabi/<debug\|release>/c_ffi_template.bin`（`make` 生成） |
| 链接 map | `target/thumbv7m-none-eabi/<debug\|release>/build/c_ffi_template-*/out/c_ffi_template.map` |
| `compile_commands.json` | `c_src/build/<Debug\|Release>/compile_commands.json` |

`make` / `make release` 最后会打印占用汇总：

```
Memory usage:
  FLASH:  25824 /  65536 B ( 39.4%)
  RAM:     2632 /  20480 B ( 12.9%)
```

- 区域大小从 [STM32F103xx_FLASH.ld](c_src/STM32F103xx_FLASH.ld) 的 `MEMORY` 段读取（换芯片只改 .ld）；
- `FLASH = text + data`，`RAM = data + bss`（bss 已包含链接脚本预留的 heap/stack）。

要看每个 section 的细节可以手动执行：

```bash
arm-none-eabi-size    target/thumbv7m-none-eabi/debug/c_ffi_template
arm-none-eabi-size -A target/thumbv7m-none-eabi/debug/c_ffi_template
```

直接用 `cargo build` 则不会打印任何占用信息。

### 4.3 build.rs 做了什么

1. 根据 cargo 的 `PROFILE` 选择 CMake preset：`debug` → `Debug`，`release` → `Release`。
2. 执行 `cmake --preset <p>` 和 `cmake --build --preset <p>`，只生成静态库。
3. 向 cargo 输出链接指令：
   - `-L c_src/build/<p>`（`-lm`）
   - `-T c_src/STM32F103xx_FLASH.ld`、`-mcpu=cortex-m3`、`-mthumb`
   - `--specs=nano.specs --specs=nosys.specs`
   - `-u _printf_float`（保留 newlib-nano 的浮点 printf，不需要可删）
   - `-Wl,--whole-archive <libMXTemplate.a> -Wl,--no-whole-archive`
   - `-Wl,--start-group -lc -lgcc -Wl,--end-group`
   - `-Wl,--gc-sections`、`-Wl,-Map=...`

三个容易踩的坑（已在 build.rs 中处理）：

- **rustc 对 no_std 目标会传 `-nodefaultlibs`**，newlib 不会自动链上，必须显式 `-lc -lgcc`。否则启动文件的 `__libc_init_array`、C 代码里的 `memcpy` 等会未定义。
- **不能按需拉取归档成员**：`startup_stm32f103xb.s` 为每个中断提供了 `.weak` 别名（指向 `Default_Handler`），而 `stm32f1xx_it.c` 里是强定义。归档成员只有被引用时才会被链接，向量表的引用已经被弱定义满足 → `stm32f1xx_it.o` 不进链接 → **所有中断都跳进 `Default_Handler` 死循环**（典型症状：`HAL_Delay()` 永不返回）。所以静态库必须用 `--whole-archive` 整体链入，未用到的代码由 `--gc-sections` 丢弃。
- **C 静态库与 libc 互相引用**：libc 的 `_write_r` 需要 `syscalls.o` 的 `_write`，用 `--start-group/--end-group` 反复扫描。

### 4.4 中断处理函数与弱符号

CubeMX 的启动文件（`startup_stm32f103xb.s`）写法是：

```asm
.weak SysTick_Handler
.thumb_set SysTick_Handler,Default_Handler
```

`stm32f1xx_it.c` 里复写的则是普通（强）定义：

```c
void SysTick_Handler(void) { HAL_IncTick(); }
```

最终二进制里谁生效取决于链接方式：

- 强定义覆盖弱定义 → 向量表指向 `stm32f1xx_it.c` 的实现，符号在 `nm` 中显示为 `T`。
- 没有强定义的中断 → 保留弱别名，依然显示为 `W`，地址等于 `Default_Handler`。
- `Reset_Handler` 本身在启动文件里也是 `.weak`，但它是唯一定义，照常生效（弱符号不等于没链接）。

验证方法：

```bash
# 看最终符号类型：T=强，W=弱
arm-none-eabi-nm -S target/thumbv7m-none-eabi/debug/c_ffi_template | grep -E ' (SysTick_Handler|USART1_IRQHandler|Default_Handler)$'

# 看向量表（SysTick 在 0x3C 偏移处）
arm-none-eabi-readelf -x .isr_vector target/thumbv7m-none-eabi/debug/c_ffi_template
```

如果看到 `W SysTick_Handler` 且地址等于 `Default_Handler`，说明中断代码没被链进来，检查是否漏了 `--whole-archive`。

Rust 侧要接管中断时，直接用 `#[unsafe(no_mangle)] pub extern "C" fn SysTick_Handler()` 定义强符号即可覆盖弱别名；C 侧 `stm32f1xx_it.c` 里的同名词函数需要删掉，否则会重复定义。

### 4.5 文件变化检测

`build.rs` 监听这些路径：`c_src/Core`、`c_src/Drivers`、`c_src/cmake`、`CMakeLists.txt`、`CMakePresets.json`、启动文件、链接脚本。

**不要监听整个 `c_src/`**，因为 CMake 输出目录 `c_src/build/` 每次构建都会变化，会导致 `build.rs` 无限重跑。

## 5. CubeMX 使用约定

### 5.1 生成代码

1. 打开 CubeMX，选择 STM32F103C8，配置外设。
2. 生成代码到 `c_src/`，工具链选 **CMake**。
3. `c_src/CMakeLists.txt` 只在首次生成时创建，之后不会被覆盖，可以放心改。

### 5.2 重新生成后需要检查的 USER CODE 区域

CubeMX 保留 `USER CODE BEGIN/END` 之间的内容，以下两处不能丢：

**`c_src/Core/Inc/main.h` — USER CODE EFP**

```c
/* USER CODE BEGIN EFP */
/* Rust 入口，由 src/main.rs 中的 rust_run() 提供 */
void rust_run(void);
/* USER CODE END EFP */
```

**`c_src/Core/Src/main.c` — USER CODE BEGIN 2**

```c
/* USER CODE BEGIN 2 */
/* CubeMX 的初始化全部完成后，进入 Rust 上层逻辑（rust_run 不返回） */
rust_run();
/* USER CODE END 2 */
```

另外，`USER CODE BEGIN Includes` 里不要再 `#include "app.h"`（`c_src/App` 已删除）。

### 5.3 新增外设后

- 如果 CubeMX 在 `main.c` 里新增了 `MX_xxx_Init()`，无需改 Rust，初始化会自动执行。
- 需要从 Rust 调用的 HAL 函数，在 `src/hal/` 下补声明。
- 需要访问的句柄（如 `hadc1`、`huart1`），用 `extern "C" { static mut hadc1: ...; }` 或加一层 C 包装函数。

## 6. Rust 侧开发

### 6.1 入口

```rust
// src/main.rs
#![no_std]
#![no_main]

#[unsafe(no_mangle)]
pub extern "C" fn rust_run() -> ! {
    app::app_init();
    app::app_run();
}
```

不要再自己实现 `__libc_init_array` 或 `main`。

### 6.2 添加 FFI 绑定

```rust
// src/hal/gpio.rs
#[allow(non_snake_case)]
pub mod ffi {
    use core::ffi::{c_uint, c_void};

    unsafe extern "C" {
        pub fn HAL_GPIO_TogglePin(GPIOx: *mut c_void, GPIO_Pin: c_uint);
    }

    pub const GPIOC: *mut c_void = 0x4001_1000 as *mut c_void;
    pub const GPIO_PIN_13: c_uint = 0x2000;
}
```

然后在 `src/peripheral/` 里封装成安全接口，避免在业务代码里出现 `unsafe`。

### 6.3 主机端单元测试

MCU target `thumbv7m-none-eabi` 没有 libtest（`test` crate），所以 `#[test]` 测试在主机（x86_64）上编译运行，不依赖硬件。

`src/main.rs` 的处理：

```rust
#![no_std]                                  // 始终 no_std
#![cfg_attr(not(test), no_main)]            // 测试时由 libtest 提供 main

#[cfg(not(test))]
use core::panic::PanicInfo;

#[cfg(test)]
mod tests;

#[cfg(not(test))]
#[panic_handler]
fn panic(_info: &PanicInfo) -> ! { loop {} } // 测试时用 std 的 panic handler
```

- crate 本身仍是 `no_std`，没有 std 预导入；测试二进制里的 std 是 libtest 链进来的。
- `build.rs` 检测到 `TARGET` 不是 `thumbv7m-none-eabi` 时直接返回，不构建 C，也不输出 ARM 链接参数。

运行：

```bash
cargo test-host      # 别名：cargo test --target x86_64-unknown-linux-gnu
```

`src/tests.rs` 里已经放了 GPIO 常量的示例测试和几个算法验证占位，直接填断言即可。

> 不要直接 `cargo test`：`.cargo/config.toml` 的 `[build] target` 固定为 MCU 目标，会报 `can't find crate for 'test'`。
>
> 另外，测试里**不要调用 `HAL_xxx`**，这些 FFI 指向 MCU 寄存器，在 x86 上会段错误。需要验证的硬件相关逻辑，先抽成不依赖 FFI 的纯函数。

## 7. printf 重定向

Rust 或 C 里调用 `printf` 时，newlib 最终走 `syscalls.c` 的 `_write` → `__io_putchar`。`__io_putchar` 在 `syscalls.c` 里只是 **weak 声明**，需要自己实现，否则 printf 会跳到地址 0。

可以用 Rust 实现（放在 `src/hal/` 或单独模块）：

```rust
use core::ffi::{c_char, c_int, c_uint};

/// CubeMX 生成的 HAL 句柄：Rust 只关心它的地址，不需要字段布局
#[repr(C)]
pub struct UART_HandleTypeDef {
    _opaque: [u8; 0],
}

unsafe extern "C" {
    pub static mut huart1: UART_HandleTypeDef;

    pub fn HAL_UART_Transmit(
        huart: *mut UART_HandleTypeDef,
        p_data: *const u8,
        size: u16,
        timeout: c_uint,
    ) -> c_int;
}

#[unsafe(no_mangle)]
pub extern "C" fn __io_putchar(ch: c_char) -> c_int {
    let byte = ch as u8;
    unsafe {
        HAL_UART_Transmit(&raw mut huart1, &byte, 1, 0xFFFF_FFFF);
    }
    ch as c_int
}
```

注意 `unsafe extern "C"` 块（Rust 2024 edition 要求），`huart1` 的实际存储由 C 链接脚本分配，Rust 侧的类型只要保证有符号地址即可。

波特率等在 CubeMX 里配置（当前 USART1：115200-8-N-1）。

## 8. 常用命令

```bash
# 构建（并生成 .hex / .bin）
make
make release

# 烧录 + 运行（.cargo/config.toml 里的 runner）
make flash
cargo run
cargo run --release

# 只生成 ELF（不生成 hex/bin）
cargo build
cargo build --release

# 主机端单元测试（不依赖硬件）
cargo test-host

# 只烧录，不接串口
probe-rs download --chip STM32F103C8 target/thumbv7m-none-eabi/debug/c_ffi_template
probe-rs reset --chip STM32F103C8

# 查看 Flash / RAM 占用（make 构建后会自动打印）
bash scripts/size-report.sh target/thumbv7m-none-eabi/debug/c_ffi_template
arm-none-eabi-size -A     target/thumbv7m-none-eabi/debug/c_ffi_template

# 清理
cargo clean
rm -rf c_src/build          # CMake 缓存/产物，可随时删除

# 只配置一次 CMake（给 clangd 生成 compile_commands.json）
cmake --preset Debug -S c_src
```

## 9. 工具链要求

```bash
rustup target add thumbv7m-none-eabi
cargo install probe-rs-tools
```

系统软件：

- `arm-none-eabi-gcc` / `arm-none-eabi-g++` / `arm-none-eabi-objcopy` / `arm-none-eabi-size`
- `cmake`（≥ 3.22）
- `ninja`
- `probe-rs`（调试器，ST-Link / DAP-Link）
- STM32CubeMX（生成代码用）

## 10. 常见问题

### `CMake Error: The current CMakeCache.txt directory ... is different`

`c_src/build/` 是从别的路径整个拷过来的，缓存里的源码路径对不上。删掉即可：

```bash
rm -rf c_src/build
cargo build
```

### `undefined reference to '__libc_init_array'` / `_write` / `_sbrk`

rustc 给 no_std 目标传了 `-nodefaultlibs`，需要 `-lc -lgcc`；同时静态库和 libc 之间要用 `--start-group`。当前 `build.rs` 已处理，若自行改链接参数请保留这两点。

### 中断不触发 / `HAL_Delay()` 永远不返回

`nm` 里如果看到 `W SysTick_Handler`、`W USART1_IRQHandler` 且地址都是 `Default_Handler`，说明 `stm32f1xx_it.o` 没被链进来（弱符号在启动文件里已被定义，归档成员不会被拉取）。确认 `build.rs` 用 `--whole-archive` 链入 `libMXTemplate.a`。

### `cargo test` 报 `can't find crate for 'test'`

`cargo test` 会按 `.cargo/config.toml` 里的 `[build] target` 为 MCU 编译测试目标，并链接 libtest（`test` crate）；裸机 target 没有 libtest，于是报 E0463。

用主机目标跑测试：

```bash
cargo test-host                 # 别名
cargo test --target x86_64-unknown-linux-gnu
```

不要直接 `cargo test`。IDE 里如果看到该错误但命令行 `cargo build` 正常，通常是 rust-analyzer 用 `cargo check --all-targets` 导致的，可加：

```json
"rust-analyzer.cargo.allTargets": false,
"rust-analyzer.check.allTargets": false
```

### clangd / IDE 报 `'stm32f1xx_hal.h' file not found`

clangd 没找到 C 的 include 路径。生成 `c_src/build/<preset>/compile_commands.json` 后，在 `compile_flags.txt` 或 `.clangd` 里指向该文件，或让 clangd 使用 `CMAKE_EXPORT_COMPILE_COMMANDS` 的输出目录。这些报错不影响 `cargo build`。

### 修改 C 代码后必须手动重编吗？

不需要。`build.rs` 监听了 `Core/`、`Drivers/` 等目录，`cargo build` 会自动重跑 CMake 并重新链接。
