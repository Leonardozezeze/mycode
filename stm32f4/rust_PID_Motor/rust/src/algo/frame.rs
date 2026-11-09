//! 串口数据帧解析（纯逻辑，不依赖硬件）
//!
//! 帧格式（11 字节，小端）：
//! ```text
//! [0]    = 0xFF       帧头
//! [1..5] = b: f32     4 字节小端
//! [5..9] = i: f32     4 字节小端
//! [9]   = 0xEE       帧尾
//! ```
//! 校验只靠帧头 `0xFF` + 帧尾 `0xEE`，中间没有校验和。

pub const FRAME_LEN: usize = 10;
pub const FRAME_HEAD: u8 = 0xFF;
pub const FRAME_TAIL: u8 = 0xEE;

/// 解析出的一帧数据
#[derive(Clone, Copy)]
pub struct Frame {
    pub b: f32,
    pub i: f32,
}

/// 字节流 → 帧 的状态机。
///
/// 主循环把每个收到的字节喂给 [`FrameParser::feed`]，
/// 凑满一帧且头尾校验通过时返回 `Some(Frame)`。
pub struct FrameParser {
    buf: [u8; FRAME_LEN],
    idx: usize,
}

impl FrameParser {
    pub const fn new() -> Self {
        Self { buf: [0; FRAME_LEN], idx: 0 }
    }

    /// 喂入一个字节；凑满一帧且头尾校验通过时返回 `Some(Frame)`。
    ///
    /// 状态机：`idx == 0` 时只认帧头；认到帧头后固定收满 `FRAME_LEN` 字节，
    /// 校验帧尾。帧尾不对就整帧丢弃，回到等待帧头状态。
    pub fn feed(&mut self, byte: u8) -> Option<Frame> {
        if self.idx == 0 {
            // 等待帧头
            if byte != FRAME_HEAD {
                return None;
            }
            self.buf[0] = byte;
            self.idx = 1;
            return None;
        }

        self.buf[self.idx] = byte;
        self.idx += 1;

        if self.idx == FRAME_LEN {
            self.idx = 0; // 收满一帧，回到等待帧头
            if self.buf[FRAME_LEN - 1] == FRAME_TAIL {
                return Some(Frame {
                    b: f32::from_le_bytes([self.buf[1], self.buf[2], self.buf[3], self.buf[4]]),
                    i: f32::from_le_bytes([self.buf[5], self.buf[6], self.buf[7], self.buf[8]]),
                });
            }
        }
        None
    }
}
