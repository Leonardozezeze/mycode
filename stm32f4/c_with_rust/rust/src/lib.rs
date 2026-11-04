#![no_std]
#![allow(dead_code)]
mod algo;
mod app;
mod peripheral;
use core::panic::PanicInfo;

use crate::peripheral::dwt::DWT;
#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    u1println!("{}",_info);
    let red = peripheral::led::Led::new(peripheral::led::LedColor::Green);
    // 调试时可以在这里放个断点 / 点灯
    loop {red.toggle();DWT::delay_ms(500);}
}

#[unsafe(no_mangle)]
pub extern "C" fn rust_run() -> ! {
    app::app_init();
    app::app_run()
}
