use super::commands::{self, CommandError, Reply};
use crate::drivers::can::CanLink;
use embassy_stm32::can::{Frame, Id};

const CAN_REQUEST_ID: u16 = 0x650;

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
            Reply::Error(error) => {
                let status = match error {
                    CommandError::UnknownCommand => 0x01,
                    CommandError::InvalidArguments | CommandError::ReplyTooLong => 0x02,
                    CommandError::Unavailable(_) => 0x03,
                };
                commands::encode_can_reply(opcode, status, &[])
            }
            Reply::Text(_) => None,
        };
        if let Some(reply) = reply {
            self.link.send(reply).await;
        }
    }
}
