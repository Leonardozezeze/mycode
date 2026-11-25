//! 推杆目标映射、平均值与运动估算，不访问硬件。

/// 含杂率越高，目标伸出长度越短；超出配置范围时夹紧到端点。
pub fn target_length_mm(impurity: f32, min_rate: f32, max_rate: f32, stroke_mm: f32) -> f32 {
    ((max_rate - impurity) / (max_rate - min_rate)).clamp(0.0, 1.0) * stroke_mm
}

/// 统计一个窗口中的样本均值；取出均值后开始下一窗口。
pub struct ImpurityAverage {
    sum: f32,
    count: u32,
}

impl ImpurityAverage {
    pub const fn new() -> Self {
        Self { sum: 0.0, count: 0 }
    }

    pub fn push(&mut self, value: f32) {
        if value.is_finite() {
            self.sum += value;
            self.count += 1;
        }
    }

    pub fn finish_window(&mut self) -> Option<f32> {
        let mean = if self.count == 0 {
            None
        } else {
            Some(self.sum / self.count as f32)
        };
        self.sum = 0.0;
        self.count = 0;
        mean
    }
}

/// 无位置传感器时按标称匀速估算，不能用作真实到位反馈。
#[derive(Clone, Copy)]
pub struct MotionEstimate {
    pub from_mm: f32,
    pub target_mm: f32,
    pub started_ms: u64,
    pub duration_ms: u32,
}

impl MotionEstimate {
    pub fn length_at(&self, now_ms: u64) -> f32 {
        if self.duration_ms == 0 {
            return self.target_mm;
        }
        let fraction = (now_ms.saturating_sub(self.started_ms) as f32 / self.duration_ms as f32)
            .clamp(0.0, 1.0);
        self.from_mm + (self.target_mm - self.from_mm) * fraction
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn impurity_endpoints_midpoint_and_clamping() {
        for (rate, expected) in [
            (0.01, 30.0),
            (0.02, 30.0),
            (0.03, 15.0),
            (0.04, 0.0),
            (0.05, 0.0),
        ] {
            assert!((target_length_mm(rate, 0.02, 0.04, 30.0) - expected).abs() < 0.0001);
        }
        let mut previous = 30.0;
        for step in 0..=100 {
            let target = target_length_mm(0.02 + step as f32 * 0.0002, 0.02, 0.04, 30.0);
            assert!((0.0..=30.0).contains(&target));
            assert!(target <= previous);
            previous = target;
        }
    }

    #[test]
    fn estimate_interpolates_both_directions_and_clamps_at_completion() {
        for (from, to) in [(0.0, 30.0), (30.0, 0.0)] {
            let motion = MotionEstimate {
                from_mm: from,
                target_mm: to,
                started_ms: 1000,
                duration_ms: 1250,
            };
            assert_eq!(motion.length_at(500), from);
            assert_eq!(motion.length_at(1000), from);
            assert_eq!(motion.length_at(1625), 15.0);
            assert_eq!(motion.length_at(2250), to);
            assert_eq!(motion.length_at(3000), to);
        }
        let stopped = MotionEstimate {
            from_mm: 15.0,
            target_mm: 15.0,
            started_ms: 0,
            duration_ms: 0,
        };
        assert_eq!(stopped.length_at(100), 15.0);
    }
    #[test]
    fn window_average_ignores_invalid_samples_and_resets() {
        let mut average = ImpurityAverage::new();
        assert_eq!(average.finish_window(), None);
        for sample in [0.02, 0.04, f32::NAN, f32::INFINITY] {
            average.push(sample);
        }
        let mean = average.finish_window().unwrap();
        assert!((mean - 0.03).abs() < 0.000001);
        assert!((target_length_mm(mean, 0.02, 0.04, 30.0) - 15.0).abs() < 0.0001);
        assert_eq!(average.finish_window(), None);
        average.push(0.04);
        assert_eq!(average.finish_window(), Some(0.04));
    }
}
