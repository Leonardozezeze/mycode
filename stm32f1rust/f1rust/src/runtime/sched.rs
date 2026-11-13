//! 周期任务原语：判断"到没到点"。
//! 零硬件依赖，时间由调用方传入。

/// 周期任务：距上次触发满 `period_ms` 毫秒即触发。
pub struct PeriodicTask {
    last: u64,
    period_ms: u64,
}

impl PeriodicTask {
    /// `last` 从 0 起算：首次触发发生在时基零点（启动）后一个周期。
    pub const fn new(period_ms: u64) -> Self {
        Self::new_at(period_ms, 0)
    }

    /// 从指定时刻起算，适用于需要对齐首次采样基准的任务。
    pub const fn new_at(period_ms: u64, now_ms: u64) -> Self {
        Self {
            last: now_ms,
            period_ms,
        }
    }

    /// 到点返回 `Some(elapsed)`（距上次触发的实测毫秒数），未到点返回 `None`。
    ///
    /// `last` 锚定实际触发时刻，实际周期 ≈ period + 任务耗时；
    /// 返回的 elapsed 是真实间隔，可直接用作 PID 等需要的 dt。
    pub fn poll(&mut self, now: u64) -> Option<u64> {
        let elapsed = now.wrapping_sub(self.last);
        if elapsed < self.period_ms {
            return None;
        }
        self.last = now;
        Some(elapsed)
    }
}
