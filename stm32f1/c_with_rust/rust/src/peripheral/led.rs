//! LED 控制模块
//!
//! 具体引脚和有效电平在 C 侧 ffi_app/Src/ffi_app.c 里定义，
//! 这里只保留面向业务的 `on/off/toggle`。

pub struct Led;
unsafe extern "C" {
    unsafe fn board_led_on();
    unsafe fn board_led_off();
    unsafe fn board_led_toggle();
}
impl Led {
    pub const fn new() -> Self {
        Self
    }

    pub fn on(&self) {
        unsafe {
            board_led_on();
        }
    }

    pub fn off(&self) {
        unsafe {
            board_led_off();
        }
    }

    pub fn toggle(&self) {
        unsafe {
            board_led_toggle();
        }
    }
}
