use super::commands::{self, CommandError, Reply};
use super::config::{CAN_REQUEST_ID, UART_MODE};
use super::shell_info;
use crate::drivers::can::CanLink;
use embassy_stm32::can::{Frame, Id};

pub struct CanShell {
    link: &'static CanLink,
}

impl CanShell {
    pub const fn new(link: &'static CanLink) -> Self {
        Self { link }
    }

    pub async fn handle_frame(&self, frame: &Frame) {
        let Id::Standard(id) = frame.id() else {
            return;
        };
        if id.as_raw() != CAN_REQUEST_ID || frame.header().rtr() {
            return;
        }

        let Some((&opcode, args)) = frame.data().split_first() else {
            return;
        };
        let reply = match commands::execute_can(opcode, args) {
            Reply::Can(frame) => Some(frame),
            Reply::CanShellInfoPage(page) => shell_info_page(opcode, page),
            Reply::Error(error) => {
                let status = match error {
                    CommandError::UnknownCommand => 0x01,
                    CommandError::InvalidArguments | CommandError::ReplyTooLong => 0x02,
                    CommandError::Unavailable(_) => 0x03,
                };
                commands::encode_can_reply(opcode, status, &[])
            }
            Reply::Text(_) | Reply::ShellInfo => None,
        };
        if let Some(reply) = reply {
            self.link.send(reply).await;
        }
    }
}

/// 成功回复由操作码、状态、两字节小端页号和最多四个 UTF-8 字节组成。
/// 页号从 0 开始。回复长度小于 8 表示读完，整页结尾时下一页为空。
fn shell_info_page(opcode: u8, page: u16) -> Option<Frame> {
    let mut data = [0; 6];
    let (index, output) = data.split_at_mut(2);
    index.copy_from_slice(&page.to_le_bytes());
    let len = shell_info::read(&UART_MODE, usize::from(page) * output.len(), output);
    commands::encode_can_reply(opcode, 0x00, &data[..2 + len])
}
