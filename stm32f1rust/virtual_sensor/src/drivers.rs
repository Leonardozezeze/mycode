//! 驱动层：封装外设操作，不做业务决策。
//! UART 使用 Embassy DMA；DWT 仅保留作显式启用的测量工具，不作应用时基。

pub mod adc_seed;
pub mod dwt;
pub mod uart;
pub mod can;