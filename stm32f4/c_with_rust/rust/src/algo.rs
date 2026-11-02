//! 纯算法与数据结构
//!
//! 这里只放与硬件无关的逻辑：不引用 `peripheral`、`ffi_app`、HAL，
//! 也不写 `extern "C"`。这样算法可以独立复用，也方便在主机上测试。

pub mod pid;
pub mod ring_buffer;
