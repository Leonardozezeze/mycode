//! 上位机遥测协议：串口基础帧、推杆扩展帧和 CAN 比例帧。

pub const FRAME_BYTES: usize = 18;
pub const ACTUATOR_FRAME_BYTES: usize = 23;
/// 虚拟品质数据的 11 位 CAN 标准帧 ID。
pub const CAN_RATE_FRAME_ID: u16 = 0x100;

/// 先含杂率，再破碎率。两个小端 f32 均使用原始比例，0.02 表示 2%。
pub fn encode_rate_payload(impurity_ratio: f32, broken_ratio: f32) -> [u8; 8] {
    let mut bytes = [0; 8];
    bytes[..4].copy_from_slice(&impurity_ratio.to_le_bytes());
    bytes[4..].copy_from_slice(&broken_ratio.to_le_bytes());
    bytes
}

#[cfg(test)]
mod tests {
    use super::encode_rate_payload;

    #[test]
    fn can_payload_matches_host_protocol() {
        assert_eq!(
            encode_rate_payload(0.03, 0.02),
            [0x8F, 0xC2, 0xF5, 0x3C, 0x0A, 0xD7, 0xA3, 0x3C],
        );
    }
}
