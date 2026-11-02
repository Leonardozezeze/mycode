#![no_std]
#![allow(dead_code)]
mod algo;
mod app;
mod peripheral;
use core::panic::PanicInfo;
#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    // 调试时可以在这里放个断点 / 点灯
    loop { core::hint::spin_loop(); }
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_run() -> ! {
    app::app_init();
    app::app_run()
}
