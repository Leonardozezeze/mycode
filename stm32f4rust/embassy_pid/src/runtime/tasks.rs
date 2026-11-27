//! 按职责组织异步任务，硬件句柄只由对应任务独占。

pub mod actuator_task;
pub mod can_shell_task;
pub mod can_task;
pub mod can_telemetry_task;
pub mod heartbeat;
pub mod speed_control;
pub mod uart_shell_task;
pub mod uart_task;
