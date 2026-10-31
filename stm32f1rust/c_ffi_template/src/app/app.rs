//! 应用主逻辑

use crate::hal::delay::ffi as delay;
use crate::hal::gpio::ffi;
use crate::peripheral::led;

/// 应用初始化
pub fn app_init() {
    // 在这里进行应用层的初始化
    // 例如：创建外设实例、初始化变量等
}

/// 应用主循环
pub fn app_run() -> ! {
    let led=led::Led::new(ffi::GPIOC, ffi::GPIO_PIN_13);
    loop {
        led.toggle();
        unsafe {
            delay::HAL_Delay(500);
        }
    }
}
