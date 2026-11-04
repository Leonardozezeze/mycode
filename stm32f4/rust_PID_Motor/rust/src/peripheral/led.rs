//! LED 控制模块
//!
//! 板载红（PF9）/ 绿（PF10）两个 LED，低电平点亮。
//! 具体引脚和有效电平在 C 侧 ffi_app/Src/led.c 里定义，
//! 这里只保留面向业务的 `on/off/toggle`。

/// 板载 LED 颜色
#[derive(Clone, Copy)]
pub enum LedColor {
    Red,
    Green,
}

pub struct Led {
    color: LedColor,
    /// 上次翻转的时刻（ms），由 `toggle_task` 维护
    last_toggle_ms: u64,
}

unsafe extern "C" {
    fn red_led_on();
    fn red_led_off();
    fn red_led_toggle();
    fn green_led_on();
    fn green_led_off();
    fn green_led_toggle();
}

impl Led {
    pub const fn new(color: LedColor) -> Self {
        Self { color, last_toggle_ms: 0 }
    }

    pub fn on(&self) {
        unsafe {
            match self.color {
                LedColor::Red => red_led_on(),
                LedColor::Green => green_led_on(),
            }
        }
    }

    pub fn off(&self) {
        unsafe {
            match self.color {
                LedColor::Red => red_led_off(),
                LedColor::Green => green_led_off(),
            }
        }
    }

    pub fn toggle(&self) {
        unsafe {
            match self.color {
                LedColor::Red => red_led_toggle(),
                LedColor::Green => green_led_toggle(),
            }
        }
    }

    /// 非阻塞周期任务：每圈调用一次，距上次翻转满 `period_ms` 才翻转。
    ///
    /// `now_ms` 由调用方传入（一般用 `DWT::update_ms()`），
    /// 这样这个函数不依赖全局时钟，也好写测试。
    pub fn toggle_task(&mut self, period_ms: u64, now_ms: u64) {
        if now_ms.saturating_sub(self.last_toggle_ms) < period_ms {
            return;
        }
        self.last_toggle_ms = now_ms;
        self.toggle();
    }
}
