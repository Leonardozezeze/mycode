//! GPIO FFI 绑定

#[allow(non_camel_case_types)]
#[allow(non_snake_case)]
pub mod ffi {
    use core::ffi::{c_uint, c_void};

    // ========== GPIO 函数 ==========
    unsafe extern "C" {
        /// 写入 GPIO 引脚
        pub fn HAL_GPIO_WritePin(
            GPIOx: *mut c_void,
            GPIO_Pin: c_uint,
            PinState: c_uint,
        );

        /// 翻转 GPIO 引脚
        pub fn HAL_GPIO_TogglePin(GPIOx: *mut c_void, GPIO_Pin: c_uint);

        /// 读取 GPIO 引脚
        pub fn HAL_GPIO_ReadPin(GPIOx: *mut c_void, GPIO_Pin: c_uint) -> c_uint;
    }

    // ========== GPIO 端口和引脚定义 ==========
    pub const GPIOA: *mut c_void = 0x4001_0800 as *mut c_void;
    pub const GPIOB: *mut c_void = 0x4001_0C00 as *mut c_void;
    pub const GPIOC: *mut c_void = 0x4001_1000 as *mut c_void;
    pub const GPIOD: *mut c_void = 0x4001_1400 as *mut c_void;

    pub const GPIO_PIN_0: c_uint = 0x0001;
    pub const GPIO_PIN_1: c_uint = 0x0002;
    pub const GPIO_PIN_2: c_uint = 0x0004;
    pub const GPIO_PIN_3: c_uint = 0x0008;
    pub const GPIO_PIN_4: c_uint = 0x0010;
    pub const GPIO_PIN_5: c_uint = 0x0020;
    pub const GPIO_PIN_6: c_uint = 0x0040;
    pub const GPIO_PIN_7: c_uint = 0x0080;
    pub const GPIO_PIN_8: c_uint = 0x0100;
    pub const GPIO_PIN_9: c_uint = 0x0200;
    pub const GPIO_PIN_10: c_uint = 0x0400;
    pub const GPIO_PIN_11: c_uint = 0x0800;
    pub const GPIO_PIN_12: c_uint = 0x1000;
    pub const GPIO_PIN_13: c_uint = 0x2000;
    pub const GPIO_PIN_14: c_uint = 0x4000;
    pub const GPIO_PIN_15: c_uint = 0x8000;

    pub const GPIO_PIN_RESET: c_uint = 0;
    pub const GPIO_PIN_SET: c_uint = 1;
}
