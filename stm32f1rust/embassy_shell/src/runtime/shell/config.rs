//! UART 和 CAN Shell 配置，修改后重新编译固件。

use super::shell::UartMode;
use embassy_stm32::can::StandardId;

/// Terminal 提供提示符、回显和行编辑，Assistant 只发送命令回复。
pub(in crate::runtime) const UART_MODE: UartMode = UartMode::Terminal;

/// 终端模式的提示符，支持 UTF-8。
pub(super) const UART_PROMPT: &str = "> ";

/// 最多保留的历史命令条数，设为 0 可关闭历史记录。
pub(super) const UART_HISTORY_CAPACITY: usize = 4;

/// 单行命令的最大 UTF-8 字节数，不含换行。
/// 提示符、命令和重绘控制序列须能装入一条 UART 发送消息。
pub(super) const UART_MAX_LINE_BYTES: usize = 64;

/// CAN Shell 接收命令的 11 位标准帧 ID。
pub(super) const CAN_REQUEST_ID: u16 = 0x401;
/// CAN Shell 发送回复的 11 位标准帧 ID。
pub(super) const CAN_REPLY_ID: u16 = 0x402;

const _: () = assert!(
    StandardId::new(CAN_REQUEST_ID).is_some() && StandardId::new(CAN_REPLY_ID).is_some(),
    "CAN_REQUEST_ID and CAN_REPLY_ID must be 11-bit standard CAN IDs",
);
