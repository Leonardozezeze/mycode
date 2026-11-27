use super::commands::{self, CommandError, Reply};
use super::config::{UART_HISTORY_CAPACITY, UART_MAX_LINE_BYTES, UART_PROMPT};
use super::shell_info;
use crate::drivers::uart::{TxMessage, UartLink};
use crate::runtime::config::uart::UART_TX_MESSAGE_BYTES;

type InputLine = heapless::Vec<u8, UART_MAX_LINE_BYTES>;
const CLEAR_LINE: &[u8] = b"\r\x1b[2K";
const SAVE_CURSOR: &[u8] = b"\x1b7";
const RESTORE_CURSOR: &[u8] = b"\x1b8";

const _: () = assert!(
    UART_PROMPT.len()
        + UART_MAX_LINE_BYTES
        + CLEAR_LINE.len()
        + SAVE_CURSOR.len()
        + RESTORE_CURSOR.len()
        <= UART_TX_MESSAGE_BYTES,
    "UART_PROMPT and UART_MAX_LINE_BYTES leave insufficient room for redraw in UART_TX_MESSAGE_BYTES",
);

enum LineState {
    Collecting,
    Discarding,
}
enum EscapeState {
    Idle,
    Esc,
    Csi,
    CsiParameters,
    Ss3,
}
pub enum UartMode {
    Terminal,
    Assistant,
}
pub struct Shell {
    uart: &'static UartLink,
    mode: UartMode,
    line: InputLine,
    cursor: usize,
    line_state: LineState,
    skip_lf: bool,
    history: heapless::Vec<InputLine, UART_HISTORY_CAPACITY>,
    history_index: Option<usize>,
    // 输入内容和光标位置要一起保存、恢复。
    draft: (InputLine, usize),
    escape_state: EscapeState,
}
impl Shell {
    pub const fn new(uart: &'static UartLink, mode: UartMode) -> Self {
        Self {
            uart,
            mode,
            line: InputLine::new(),
            cursor: 0,
            line_state: LineState::Collecting,
            skip_lf: false,
            history: heapless::Vec::new(),
            history_index: None,
            draft: (InputLine::new(), 0),
            escape_state: EscapeState::Idle,
        }
    }

    pub(in crate::runtime) async fn show_prompt(&self) {
        if self.is_terminal() {
            self.send_response(&[UART_PROMPT.as_bytes()]).await;
        }
    }

    fn send_uart(&self, parts: &[&[u8]]) {
        let mut message = TxMessage::new();
        for part in parts {
            if message.extend_from_slice(part).is_err() {
                return;
            }
        }
        let _ = self.uart.try_send(message);
    }

    async fn send_response(&self, parts: &[&[u8]]) {
        let mut message = TxMessage::new();
        for mut part in parts.iter().copied() {
            while !part.is_empty() {
                let len = part.len().min(UART_TX_MESSAGE_BYTES - message.len());
                let (chunk, rest) = part.split_at(len);
                message.extend_from_slice(chunk).expect("UART chunk fits");
                part = rest;
                if message.is_full() {
                    self.uart.send(core::mem::take(&mut message)).await;
                }
            }
        }
        if !message.is_empty() {
            self.uart.send(message).await;
        }
    }

    pub async fn handle_command(&self, text: &str) {
        let reply = commands::execute(text);
        let text = match &reply {
            Reply::Text(text) => text.as_str(),
            Reply::Can(_) | Reply::CanShellInfoPage(_) => return,
            Reply::ShellInfo => return self.show_shell_info().await,
            Reply::Error(CommandError::UnknownCommand) => "unknown command",
            Reply::Error(CommandError::InvalidArguments) => "invalid arguments",
            Reply::Error(CommandError::ReplyTooLong) => "reply too long",
            Reply::Error(CommandError::Unavailable(message)) => message,
        };
        self.send_response(&[text.as_bytes(), b"\r\n"]).await;
    }

    async fn show_shell_info(&self) {
        let mut buffer = [0; UART_TX_MESSAGE_BYTES];
        let mut offset = 0;
        loop {
            let len = shell_info::read(&self.mode, offset, &mut buffer);
            if len > 0 {
                self.uart
                    .send_bytes(&buffer[..len])
                    .await
                    .expect("UART chunk fits");
            }
            if len < buffer.len() {
                break;
            }
            offset += len;
        }
    }

    pub async fn feed(&mut self, bytes: &[u8]) {
        let terminal = self.is_terminal();
        let mut pending_echo = InputLine::new();
        for &byte in bytes {
            // CRLF 可能分在两次 feed() 里，只提交一次。
            let skip_lf = core::mem::replace(&mut self.skip_lf, byte == b'\r');
            if skip_lf && byte == b'\n' {
                continue;
            }
            match (&self.line_state, byte) {
                (_, b'\r' | b'\n') => {
                    self.flush_echo(&mut pending_echo);
                    self.finish_line().await;
                }
                (LineState::Discarding, _) => {}
                (LineState::Collecting, b'\x08' | b'\x7f') if terminal => {
                    self.flush_echo(&mut pending_echo);
                    self.escape_state = EscapeState::Idle;
                    self.backspace();
                }
                (LineState::Collecting, _) => {
                    if terminal {
                        if byte == b'\x1b' {
                            self.flush_echo(&mut pending_echo);
                        }
                        if self.consume_escape(byte) {
                            continue;
                        }
                    }
                    if !self.insert_byte(byte) {
                        self.flush_echo(&mut pending_echo);
                        self.discard_line();
                    } else if terminal {
                        // 只存成功插入的字节，回显缓冲不会超过一行的容量。
                        let _ = pending_echo.push(byte);
                    }
                }
            }
        }
        self.flush_echo(&mut pending_echo);
    }

    async fn finish_line(&mut self) {
        if self.is_terminal() {
            self.send_response(&[b"\r\n"]).await;
        }
        if let Ok(text) = core::str::from_utf8(&self.line)
            && !text.is_empty()
        {
            if self.is_terminal() && UART_HISTORY_CAPACITY > 0 && !text.trim().is_empty() {
                if self.history.is_full() {
                    self.history.remove(0);
                }
                let _ = self.history.push(self.line.clone());
            }
            self.handle_command(text).await;
        }
        self.reset_line(LineState::Collecting);
        self.show_prompt().await;
    }

    fn insert_byte(&mut self, byte: u8) -> bool {
        if self.line.insert(self.cursor, byte).is_err() {
            return false;
        }
        self.cursor += 1;
        true
    }

    fn flush_echo(&self, pending: &mut InputLine) {
        if pending.is_empty() {
            return;
        }

        if self.cursor == self.line.len() {
            self.echo(&[pending.as_slice()]);
        } else if core::str::from_utf8(&self.line).is_ok() {
            // 行中插入的一批字符只重画一次，等多字节字符收完整再重画。
            self.redraw_line();
        }
        pending.clear();
    }

    fn previous_char_start(&self) -> Option<usize> {
        let last_byte = self.cursor.checked_sub(1)?;
        // 有效的 UTF-8 按字符删除。遇到无效输入时只删末尾一个字节。
        let start = core::str::from_utf8(&self.line[..self.cursor])
            .ok()
            .and_then(|line| line.char_indices().next_back())
            .map_or(last_byte, |(index, _)| index);
        Some(start)
    }

    fn backspace(&mut self) {
        let Some(start) = self.previous_char_start() else {
            return;
        };
        let removed = self.cursor - start;
        let len = self.line.len();
        let erase_one_column = self.cursor == len && removed == 1;
        self.line.copy_within(self.cursor.., start);
        self.line.truncate(len - removed);
        self.cursor = start;

        if erase_one_column {
            self.echo(&[b"\x08 \x08"]);
        } else {
            self.redraw_line();
        }
    }

    fn cursor_left(&mut self) {
        if let Some(start) = self.previous_char_start() {
            self.cursor = start;
            self.redraw_line();
        }
    }

    fn cursor_right(&mut self) {
        let suffix = &self.line[self.cursor..];
        if suffix.is_empty() {
            return;
        }
        self.cursor += core::str::from_utf8(suffix)
            .ok()
            .and_then(|text| text.chars().next())
            .map_or(1, char::len_utf8);
        self.redraw_line();
    }

    fn is_terminal(&self) -> bool {
        matches!(self.mode, UartMode::Terminal)
    }

    fn echo(&self, parts: &[&[u8]]) {
        if self.is_terminal() {
            self.send_uart(parts);
        }
    }

    pub fn discard_line(&mut self) {
        self.reset_line(LineState::Discarding);
        self.skip_lf = false;
    }

    fn reset_line(&mut self, state: LineState) {
        self.line.clear();
        self.cursor = 0;
        self.line_state = state;
        self.history_index = None;
        self.draft = (InputLine::new(), 0);
        self.escape_state = EscapeState::Idle;
    }

    fn history_up(&mut self) {
        let Some(latest) = self.history.len().checked_sub(1) else {
            return;
        };

        let index = match self.history_index {
            None => {
                self.draft = (self.line.clone(), self.cursor);
                latest
            }
            Some(index) => index.saturating_sub(1),
        };

        self.show_history(index);
    }

    fn history_down(&mut self) {
        let Some(index) = self.history_index else {
            return;
        };

        let next = index + 1;
        if next < self.history.len() {
            self.show_history(next);
        } else {
            (self.line, self.cursor) = core::mem::take(&mut self.draft);
            self.history_index = None;
            self.redraw_line();
        }
    }

    fn show_history(&mut self, index: usize) {
        self.line = self.history[index].clone();
        self.cursor = self.line.len();
        self.history_index = Some(index);
        self.redraw_line();
    }

    fn redraw_line(&self) {
        let (prefix, suffix) = self.line.split_at(self.cursor);
        let prompt = UART_PROMPT.as_bytes();
        if suffix.is_empty() {
            self.echo(&[CLEAR_LINE, prompt, prefix]);
        } else {
            // 保存前缀末尾的光标，写出后缀，再还原光标位置。
            self.echo(&[
                CLEAR_LINE,
                prompt,
                prefix,
                SAVE_CURSOR,
                suffix,
                RESTORE_CURSOR,
            ]);
        }
    }

    fn consume_escape(&mut self, byte: u8) -> bool {
        let state = core::mem::replace(&mut self.escape_state, EscapeState::Idle);

        match (state, byte) {
            (_, b'\x1b') => self.escape_state = EscapeState::Esc,
            (EscapeState::Esc, b'[') => self.escape_state = EscapeState::Csi,
            (EscapeState::Esc, b'O') => self.escape_state = EscapeState::Ss3,
            (EscapeState::Csi | EscapeState::Ss3, b'A') => self.history_up(),
            (EscapeState::Csi | EscapeState::Ss3, b'B') => self.history_down(),
            (EscapeState::Csi | EscapeState::Ss3, b'C') => self.cursor_right(),
            (EscapeState::Csi | EscapeState::Ss3, b'D') => self.cursor_left(),
            (EscapeState::Csi | EscapeState::CsiParameters, 0x20..=0x3f) => {
                self.escape_state = EscapeState::CsiParameters;
            }
            (EscapeState::Csi | EscapeState::CsiParameters | EscapeState::Ss3, 0x40..=0x7e) => {}
            _ => return false,
        }

        true
    }
}
