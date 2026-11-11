//! 驱动层：封装器件操作（电机 PWM、UART 收发等），不做业务决策。
//! 待添加的成员见 ARCHITECTURE.md 路线图。

pub mod dwt;
pub mod uart;
pub mod motor;
pub mod enc;