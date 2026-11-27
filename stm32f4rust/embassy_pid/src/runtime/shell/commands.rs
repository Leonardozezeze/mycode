use super::config::CAN_REPLY_ID;
use crate::runtime::config::uart::UART_TX_MESSAGE_BYTES;
use core::fmt::{self, Write};
use embassy_stm32::can::Frame;

mod actuator_commands;
mod can_commands;
mod speed_commands;
mod system;
mod uart_commands;

static COMMANDS: &[Command] = &[
    system::PING,
    system::ECHO,
    uart_commands::UART_STATS,
    can_commands::CAN_STATS,
    system::SHELL_INFO,
    speed_commands::SPEED,
    speed_commands::PWM,
    speed_commands::PID,
    speed_commands::ENCODER,
    speed_commands::STOP,
    actuator_commands::ACTUATOR,
];

pub(in crate::runtime) use can_commands::bind_can1;
pub(in crate::runtime) use uart_commands::bind_uart1;

// Shell 在发送回复时还要追加 CRLF。
const REPLY_TEXT_BYTES: usize = UART_TX_MESSAGE_BYTES - 2;

struct Command {
    name: &'static str,
    // 帮助文本使用单行，供 <命令> help 和 shell-info 共用。
    help: &'static str,
    /// 设为 None 时只通过 UART 调用。
    can_opcode: Option<u8>,
    run: fn(Request<'_>) -> Reply,
}

enum Request<'a> {
    Uart(&'a str),
    Can { opcode: u8, args: &'a [u8] },
}

pub(super) enum Reply {
    Text(heapless::String<REPLY_TEXT_BYTES>),
    Can(Frame),
    ShellInfo,
    CanShellInfoPage(u16),
    Error(CommandError),
}

pub(super) enum CommandError {
    UnknownCommand,
    InvalidArguments,
    ReplyTooLong,
    Unavailable(&'static str),
}

impl Reply {
    fn text(text: &str) -> Self {
        Self::formatted(format_args!("{text}"))
    }

    fn formatted(args: fmt::Arguments<'_>) -> Self {
        let mut text = heapless::String::new();
        match text.write_fmt(args) {
            Ok(()) => Self::Text(text),
            Err(_) => Self::Error(CommandError::ReplyTooLong),
        }
    }

    fn can(opcode: u8, data: &[u8]) -> Self {
        match encode_can_reply(opcode, 0x00, data) {
            Some(frame) => Self::Can(frame),
            None => Self::Error(CommandError::ReplyTooLong),
        }
    }
}

pub(super) fn encode_can_reply(opcode: u8, status: u8, data: &[u8]) -> Option<Frame> {
    let mut payload = [0u8; 8];
    payload[0] = opcode;
    payload[1] = status;
    payload[2..].get_mut(..data.len())?.copy_from_slice(data);
    Frame::new_standard(CAN_REPLY_ID, &payload[..2 + data.len()]).ok()
}

pub(super) fn command_info() -> impl Iterator<Item = (&'static str, Option<u8>, &'static str)> {
    COMMANDS
        .iter()
        .map(|entry| (entry.name, entry.can_opcode, entry.help))
}

pub(super) fn execute(text: &str) -> Reply {
    let text = text.trim();
    let (command, args) = text.split_once(' ').unwrap_or((text, ""));
    let Some(entry) = COMMANDS.iter().find(|entry| entry.name == command) else {
        return Reply::Error(CommandError::UnknownCommand);
    };

    if args == "help" {
        Reply::text(entry.help)
    } else {
        (entry.run)(Request::Uart(args))
    }
}

pub(super) fn execute_can(opcode: u8, args: &[u8]) -> Reply {
    let Some(entry) = COMMANDS
        .iter()
        .find(|entry| entry.can_opcode == Some(opcode))
    else {
        return Reply::Error(CommandError::UnknownCommand);
    };
    (entry.run)(Request::Can { opcode, args })
}
