//! LED 控制模块

use crate::hal::gpio::ffi;
use core::ffi::{c_uint, c_void};

/// LED 结构体
pub struct Led {
    port: *mut c_void,
    pin: c_uint,
}

impl Led {
    pub fn new(port: *mut c_void, pin: c_uint) -> Self {
        Self { port, pin }
    }

    pub fn on(&self) {
        unsafe {
            ffi::HAL_GPIO_WritePin(self.port, self.pin, ffi::GPIO_PIN_SET);
        }
    }

    pub fn off(&self) {
        unsafe {
            ffi::HAL_GPIO_WritePin(self.port, self.pin, ffi::GPIO_PIN_RESET);
        }
    }

    pub fn toggle(&self) {
        unsafe {
            ffi::HAL_GPIO_TogglePin(self.port, self.pin);
        }
    }
}
