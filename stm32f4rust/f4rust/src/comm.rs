//! 通信基础设施层：环形缓冲与帧协议，零硬件依赖（禁引入 HAL/PAC）。

pub mod proto;
pub mod ringbuf;
