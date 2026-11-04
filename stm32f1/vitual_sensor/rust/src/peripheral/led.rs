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
    pub fn toggle_task(&self, now_ms: u32) {
        static mut LAST_TOGGLE: u32 = 0;
        const PERIOD_MS: u32 = 500;
        unsafe {
            if now_ms.wrapping_sub(LAST_TOGGLE) > PERIOD_MS {
                LAST_TOGGLE = now_ms;
                self.toggle();
            }
        }
    }
}
