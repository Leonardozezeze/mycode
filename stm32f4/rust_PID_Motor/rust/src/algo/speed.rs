//! 转速计算：纯数学，不依赖任何硬件。
//!
//! 输入是"采样周期内的计数增量 + 时间间隔"，输出 RPM。
//! 增量来源（编码器 / 霍尔 / CAN 上报）由调用方决定，本模块不感知。

/// 把每采样周期的计数增量换算成转速（RPM）
pub struct SpeedMeter {
    /// 每转计数（编码器线数 × 4 倍频）
    counts_per_rev: f32,
}

impl SpeedMeter {
    /// `counts_per_rev`：电机转一圈产生的计数值，例如 500 线 × 4 倍频 = 2000.0
    pub const fn new(counts_per_rev: f32) -> Self {
        Self { counts_per_rev }
    }

    /// `delta`：本采样周期的计数增量；`dt_ms`：距上次采样的间隔（毫秒）
    ///
    /// RPM = (delta / counts_per_rev) 转 × (60000 / dt_ms) 每分钟
    pub fn m_update(&mut self, delta: i32, dt_ms: f32) -> f32 {
        delta as f32 * 60000.0 / (self.counts_per_rev * dt_ms)
    }
}
