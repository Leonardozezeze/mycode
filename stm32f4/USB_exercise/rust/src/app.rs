use crate::{cdc_println, peripheral::{self, delay::Delay}};

unsafe extern "C"{
    unsafe fn board_drive_init();
}
/// 应用初始化
pub fn app_init() {
    unsafe {
        board_drive_init();
    }
    cdc_println!("APP Init Okay!!!");
}

/// 应用主循环
pub fn app_run() -> ! {
    let led=peripheral::led::Led::new();
    let mut count: u32=0;
    loop {
        led.toggle();
        Delay::ms(1000);
        cdc_println!("led toggle:{}",count);
        count+=1;
    }
}
