//! STM32F1 Rust 模板 - CubeMX + FFI 开发模式
//!
//! 这个模板使用 C 侧提供的启动代码和链接脚本
//! C 的 main 函数调用 rust_run()

#![cfg_attr(not(test), no_std)]
// 固件构建：no_main；主机测试由 test harness 提供 main
#![cfg_attr(not(test), no_main)]
#![allow(dead_code)]

#[cfg(not(test))]
use core::panic::PanicInfo;

// 模块声明
mod hal;
mod peripheral;
mod app;

/// 主机端单元测试（`cargo test-host`，不依赖硬件）
#[cfg(test)]
mod tests;

/// Panic 处理函数（仅固件构建；测试时用 std 自带的，否则 lang item 重复）
#[cfg(not(test))]
#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

/// Rust 主函数
/// 由 C 的 main 函数调用
/// 同时负责初始化和主循环
#[unsafe(no_mangle)]
pub extern "C" fn rust_run() -> ! {
    app::app_init();
    app::app_run();
}
