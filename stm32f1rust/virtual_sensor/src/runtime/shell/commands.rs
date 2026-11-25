use crate::runtime::config;
use core::fmt::{self, Write};
use embassy_stm32::can::Frame;

mod can_commands;
mod system;
mod uart_commands;

static COMMANDS: &[Command] = &[
    system::PING,
    system::ECHO,
    uart_commands::UART_STATS,
    can_commands::CAN_STATS,
];

pub(in crate::runtime) use can_commands::bind_can1;
pub(in crate::runtime) use uart_commands::bind_uart1;

// Shell 在发送回复时还要追加 CRLF。
const REPLY_TEXT_BYTES: usize = config::UART_TX_MESSAGE_BYTES - 2;
const CAN_REPLY_ID: u16 = 0x651;

struct Command {
    name: &'static str,
    help: &'static str,
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
