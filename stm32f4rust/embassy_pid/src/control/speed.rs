//! 测频法测速与一阶低通滤波。

use core::time::Duration;

#[derive(Debug)]
pub struct SpeedEstimate {
    /// 本次采样换算出的带方向平均转速。
    pub raw_rpm: f32,
    /// 滤波后的带方向转速。关闭滤波时等于 `raw_rpm`。
    pub rpm: f32,
}

pub struct SpeedEstimator {
    counts_per_rev: f32,
    filter_time_secs: f32,
    previous_rpm: Option<f32>,
}

impl SpeedEstimator {
    /// `counts_per_rev` 是被测轴每转的编码器计数，须为有限正数。
    /// `filter_time` 是低通时间常数，传入 `Duration::ZERO` 可关闭滤波。
    pub fn new(counts_per_rev: f32, filter_time: Duration) -> Option<Self> {
        if !counts_per_rev.is_finite() || counts_per_rev <= 0.0 {
            return None;
        }

        Some(Self {
            counts_per_rev,
            filter_time_secs: filter_time.as_secs_f32(),
            previous_rpm: None,
        })
    }

    /// 用本次采样的净计数和实际用时更新转速。无效采样时保留滤波状态。
    pub fn update(&mut self, counts: i16, elapsed: Duration) -> Option<SpeedEstimate> {
        if elapsed.is_zero() {
            return None;
        }

        let dt = elapsed.as_secs_f32();
        let raw_rpm = f32::from(counts) * 60.0 / dt / self.counts_per_rev;
        if !raw_rpm.is_finite() {
            return None;
        }

        // 首次采样直接作为滤波初值。实际间隔变化时同步更新权重。
        let rpm = match self.previous_rpm {
            Some(previous) => {
                let alpha = dt / (self.filter_time_secs + dt);
                (1.0 - alpha) * previous + alpha * raw_rpm
            }
            None => raw_rpm,
        };
        self.previous_rpm = Some(rpm);

        Some(SpeedEstimate { raw_rpm, rpm })
    }
}
