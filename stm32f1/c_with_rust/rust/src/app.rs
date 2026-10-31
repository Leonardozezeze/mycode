use crate::{peripheral::{self, delay::Delay}, u1println};

unsafe extern "C"{
    unsafe fn board_drive_init();
}
/// 应用初始化
pub fn app_init() {
    unsafe {
        board_drive_init();
    }
    u1println!("APP Init Okay!!!");
}

/// 应用主循环
pub fn app_run() -> ! {
    let led=peripheral::led::Led::new();
    let mut count: u32=0;
    loop {
        led.toggle();
        Delay::ms(1000);
        u1println!("led toggle:{}",count);
        count+=1;
    }
}
