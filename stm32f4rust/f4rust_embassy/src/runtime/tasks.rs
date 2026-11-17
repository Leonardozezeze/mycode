//! 按职责组织异步任务，硬件句柄只由对应任务独占。

pub mod debug_uart;
pub mod heartbeat;
pub mod uart;
