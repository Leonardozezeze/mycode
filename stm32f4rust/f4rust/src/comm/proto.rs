//! 帧协议解析状态机：字节流 → 完整帧。只认字节，不碰硬件。
//!
//! 状态机骨架：WaitHead → WaitLen → Payload → Checksum → 出帧。
//!
//! TODO: `enum ParseState`、`struct Parser`、`fn feed(&mut self, bytes: &[u8]) -> Option<Frame>`
