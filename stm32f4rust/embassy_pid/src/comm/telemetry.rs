//! 上位机的电机转速遥测协议。

/// 电机转速的 CAN 标准帧 ID。
pub const CAN_SPEED_FRAME_ID: u16 = 0x101;

/// 两个转速均以 RPM 为单位，按实测、目标顺序编码为小端 f32。
pub fn encode_speed_payload(measured_rpm: f32, target_rpm: f32) -> [u8; 8] {
    let mut payload = [0; 8];
    payload[..4].copy_from_slice(&measured_rpm.to_le_bytes());
    payload[4..].copy_from_slice(&target_rpm.to_le_bytes());
    payload
}
