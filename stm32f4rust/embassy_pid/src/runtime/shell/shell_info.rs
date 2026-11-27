use super::commands;
use super::config::{
    CAN_REPLY_ID, CAN_REQUEST_ID, UART_HISTORY_CAPACITY, UART_MAX_LINE_BYTES, UART_PROMPT,
};
use super::shell::UartMode;
use crate::runtime::config::{can::CAN1_BITRATE, uart::UART1_BAUD_RATE};
use core::fmt::{self, Write};

/// 读取指定字节范围，CAN 和 UART 共用同一份 UTF-8 报告。
pub(super) fn read(mode: &UartMode, offset: usize, output: &mut [u8]) -> usize {
    let mut chunk = ReportChunk {
        skip: offset,
        output,
        written: 0,
    };
    // 格式化遇到已写满的输出缓冲区就提前结束。
    let _ = write_report(&mut chunk, mode);
    chunk.written
}

fn write_report(output: &mut impl Write, mode: &UartMode) -> fmt::Result {
    let mode = match mode {
        UartMode::Terminal => "terminal",
        UartMode::Assistant => "assistant",
    };
    write!(output, "shell-info begin\r\n")?;
    write!(
        output,
        "can_request_id=0x{CAN_REQUEST_ID:03X} can_reply_id=0x{CAN_REPLY_ID:03X} can_bitrate={CAN1_BITRATE}\r\n",
    )?;
    write!(
        output,
        "uart_mode={mode} uart_baud_rate={UART1_BAUD_RATE} uart_max_line_bytes={UART_MAX_LINE_BYTES} uart_history_capacity={UART_HISTORY_CAPACITY}\r\n",
    )?;
    write!(output, "uart_prompt={UART_PROMPT:?}\r\n")?;
    for (name, opcode, help) in commands::command_info() {
        if let Some(opcode) = opcode {
            write!(
                output,
                "command name={name} opcode=0x{opcode:02X} help={help}\r\n"
            )?;
        } else {
            write!(output, "command name={name} opcode=none help={help}\r\n")?;
        }
    }
    write!(output, "shell-info end\r\n")
}

struct ReportChunk<'a> {
    skip: usize,
    output: &'a mut [u8],
    written: usize,
}

impl Write for ReportChunk<'_> {
    fn write_str(&mut self, text: &str) -> fmt::Result {
        let bytes = text.as_bytes();
        let skip = self.skip.min(bytes.len());
        self.skip -= skip;
        let bytes = &bytes[skip..];

        let len = bytes.len().min(self.output.len() - self.written);
        self.output[self.written..self.written + len].copy_from_slice(&bytes[..len]);
        self.written += len;
        if self.written == self.output.len() {
            Err(fmt::Error)
        } else {
            Ok(())
        }
    }
}
