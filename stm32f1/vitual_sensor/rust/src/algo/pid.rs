//! 增量式 PID
//!
//! 增量式公式：
//!
//! ```text
//! Δu(k) = Kp·[e(k) − e(k−1)]
//!       + Ki·e(k)
//!       + Kd·[e(k) − 2·e(k−1) + e(k−2)]
//!
//! u(k) = clamp(u(k−1) + Δu(k), out_min, out_max)
//! ```
//!
//! 三个增益直接按采样周期整定：Ki、Kd 已经包含采样周期 T，
//! 调用方每来一次采样调用一次 [`Pid::update`] 即可。
//!
//! 注意：Cortex-M3 没有硬件 FPU，`f32` 运算会走软件浮点。

pub struct Pid {
    kp: f32,
    ki: f32,
    kd: f32,
    /// e(k-1)
    prev_error: f32,
    /// e(k-2)
    prev_prev_error: f32,
    /// u(k-1)，累计输出
    output: f32,
    out_min: f32,
    out_max: f32,
}

impl Pid {
    /// 创建一个输出不限幅的 PID
    pub const fn new(kp: f32, ki: f32, kd: f32) -> Self {
        Self {
            kp,
            ki,
            kd,
            prev_error: 0.0,
            prev_prev_error: 0.0,
            output: 0.0,
            out_min: f32::NEG_INFINITY,
            out_max: f32::INFINITY,
        }
    }

    pub fn set_gains(&mut self, kp: f32, ki: f32, kd: f32) {
        self.kp = kp;
        self.ki = ki;
        self.kd = kd;
    }

    /// 设置输出限幅；`min > max` 时忽略本次设置
    pub fn set_output_limits(&mut self, min: f32, max: f32) {
        if min <= max {
            self.out_min = min;
            self.out_max = max;
        }
    }

    /// 输入目标值和测量值，返回本周期累计输出 u(k)
    pub fn update(&mut self, setpoint: f32, measured: f32) -> f32 {
        let error = setpoint - measured;

        let delta = self.kp * (error - self.prev_error)
            + self.ki * error
            + self.kd * (error - 2.0 * self.prev_error + self.prev_prev_error);

        self.prev_prev_error = self.prev_error;
        self.prev_error = error;

        let mut out = self.output + delta;
        if out > self.out_max {
            out = self.out_max;
        }
        if out < self.out_min {
            out = self.out_min;
        }
        self.output = out;
        out
    }

    /// 清空历史误差和累计输出（例如重新使能控制时调用）
    pub fn reset(&mut self) {
        self.prev_error = 0.0;
        self.prev_prev_error = 0.0;
        self.output = 0.0;
    }

    /// 最近一次 `update` 的输出
    pub fn output(&self) -> f32 {
        self.output
    }
}
