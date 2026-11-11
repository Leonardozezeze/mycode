//! UART5 传感器协议：0xFF + 两个 4 字节 IEEE 754 float + 0xEE。
//! 逐字节解析，允许一帧分多次到达，也允许多帧连续到达；不依赖 UART/HAL。

const FRAME_HEAD: u8 = 0xFF;
const FRAME_TAIL: u8 = 0xEE;
const FRAME_LEN: usize = 10;

#[derive(Debug, Clone, Copy)]
pub enum ByteOrder {
    LittleEndian,
    BigEndian,
}

#[derive(Debug, Clone, Copy, PartialEq)]
pub struct SensorFrame {
    /// 破碎率 b。
    pub value1: f32,
    /// 含杂率 i。
    pub value2: f32,
}

/// USART1 回传：FF | b | i | 实测 RPM | 目标 RPM | EE，共 18 字节，float 均为小端。
pub fn encode_telemetry(frame: SensorFrame, rpm: f32, target_rpm: f32) -> [u8; 18] {
    let mut bytes = [0u8; 18];
    bytes[0] = FRAME_HEAD;
    bytes[1..5].copy_from_slice(&frame.value1.to_le_bytes());
    bytes[5..9].copy_from_slice(&frame.value2.to_le_bytes());
    bytes[9..13].copy_from_slice(&rpm.to_le_bytes());
    bytes[13..17].copy_from_slice(&target_rpm.to_le_bytes());
    bytes[17] = FRAME_TAIL;
    bytes
}

pub struct SensorParser {
    bytes: [u8; FRAME_LEN],
    len: usize,
    byte_order: ByteOrder,
}

impl SensorParser {
    pub const fn new(byte_order: ByteOrder) -> Self {
        Self {
            bytes: [0; FRAME_LEN],
            len: 0,
            byte_order,
        }
    }

    /// 丢弃未完成的帧，重新寻找帧头。
    pub fn reset(&mut self) {
        self.len = 0;
    }

    /// 输入一个字节，完整帧到达时返回两个 float。
    /// 中间 8 字节是原始浮点数据，可以包含 0xFF 或 0xEE，不当作分隔符。
    pub fn feed(&mut self, byte: u8) -> Option<SensorFrame> {
        if self.len == 0 && byte != FRAME_HEAD {
            return None;
        }
        self.bytes[self.len] = byte;
        self.len += 1;
        if self.len < FRAME_LEN {
            return None;
        }

        if self.bytes[FRAME_LEN - 1] == FRAME_TAIL {
            let value1 = self.decode([self.bytes[1], self.bytes[2], self.bytes[3], self.bytes[4]]);
            let value2 = self.decode([self.bytes[5], self.bytes[6], self.bytes[7], self.bytes[8]]);
            self.reset();
            return Some(SensorFrame { value1, value2 });
        }

        // 帧尾错误：原候选帧里可能已包含下一帧的开头，保留它以重新同步。
        if let Some(offset) = self.bytes[1..].iter().position(|&b| b == FRAME_HEAD) {
            let start = offset + 1;
            self.bytes.copy_within(start..FRAME_LEN, 0);
            self.len = FRAME_LEN - start;
        } else {
            self.reset();
        }
        None
    }

    fn decode(&self, bytes: [u8; 4]) -> f32 {
        match self.byte_order {
            ByteOrder::LittleEndian => f32::from_le_bytes(bytes),
            ByteOrder::BigEndian => f32::from_be_bytes(bytes),
        }
    }
}
