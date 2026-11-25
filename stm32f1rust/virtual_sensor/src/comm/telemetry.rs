//! 上位机遥测协议：串口基础帧、推杆扩展帧和 CAN 比例帧。

pub const FRAME_BYTES: usize = 18;
pub const ACTUATOR_FRAME_BYTES: usize = 23;
pub const CAN_RATE_FRAME_ID: u16 = 0x123;

/// 先破碎率，再含杂率。两个小端 f32 均使用原始比例，0.02 表示 2%。
pub fn encode_rate_payload(broken_ratio: f32, impurity_ratio: f32) -> [u8; 8] {
    let mut bytes = [0; 8];
    bytes[..4].copy_from_slice(&broken_ratio.to_le_bytes());
    bytes[4..].copy_from_slice(&impurity_ratio.to_le_bytes());
    bytes
}
