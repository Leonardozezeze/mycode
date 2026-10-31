//! 延时函数 FFI 绑定

#[allow(non_snake_case)]
pub mod ffi {
    use core::ffi::c_uint;

    unsafe extern "C" {
        /// 毫秒延时
        pub fn HAL_Delay(Delay: c_uint);
    }
}
