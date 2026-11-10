//! 增量式（速度式）PID 控制器。
//!
//! 每拍计算输出增量并累加到上一拍输出：
//!
//! ```text
//! Δu(k) = Kp·[e(k) − e(k−1)] + Ki·e(k)·dt + Kd·[e(k) − 2e(k−1) + e(k−2)]/dt
//! u(k)  = clamp( u(k−1) + Δu(k) )
//! ```
//!
//! 使用约定：调用方必须以恒定的 `dt` 调用 [`Pid::update`]。
//!
//! 增量式没有独立的积分状态——输出累计值本身就是积分，
//! 被限幅卡住时累计自然停止，结构上不存在积分饱和。

/// 增量式 PID 控制器
#[derive(Debug, Clone)]
pub struct Pid {
    kp: f32,
    ki: f32,
    kd: f32,
    out_min: f32,
    out_max: f32,
    /// 上一拍输出 u(k-1)，即累计值
    out_prev: f32,
    /// e(k-1)
    err_prev: f32,
    /// e(k-2)
    err_prev2: f32,
}

impl Pid {
    /// 新建 PID。`out_min`/`out_max` 为输出限幅（例如 PWM 占空比 -100.0..100.0）
    pub fn new(kp: f32, ki: f32, kd: f32, out_min: f32, out_max: f32) -> Self {
        Self {
            kp,
            ki,
            kd,
            out_min,
            out_max,
            out_prev: 0.0,
            err_prev: 0.0,
            err_prev2: 0.0,
        }
    }

    /// 推进一拍，返回限幅后的绝对控制量。
    ///
    /// * `setpoint` 目标值
    /// * `measured` 当前测量值
    /// * `dt`       距上次调用的时间步长（秒），必须 > 0 且每次一致
    pub fn update(&mut self, setpoint: f32, measured: f32, dt: f32) -> f32 {
        let err = setpoint - measured;

        // 微分项用误差差分：setpoint 阶跃会带来一拍微分冲击，随后消失
        let delta = self.kp * (err - self.err_prev)
            + self.ki * err * dt
            + self.kd * (err - 2.0 * self.err_prev + self.err_prev2) / dt;

        // 累加并限幅：饱和期间 out_prev 停在限幅值上，累计自动停止
        let out = (self.out_prev + delta).clamp(self.out_min, self.out_max);

        self.err_prev2 = self.err_prev;
        self.err_prev = err;
        self.out_prev = out;

        out
    }

    /// 清零内部状态（输出回到 0）。参数切换或故障恢复后调用
    pub fn reset(&mut self) {
        self.out_prev = 0.0;
        self.err_prev = 0.0;
        self.err_prev2 = 0.0;
    }

    /// 在线调整增益（例如上位机串口调参）
    pub fn set_gains(&mut self, kp: f32, ki: f32, kd: f32) {
        self.kp = kp;
        self.ki = ki;
        self.kd = kd;
    }
}
