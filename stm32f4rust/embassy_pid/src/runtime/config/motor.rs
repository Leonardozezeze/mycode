//! 电机 PWM 参数。

/// TIM3 的计数频率，ARR 为 1000 时，PWM 频率约为 999 Hz。
pub const MOTOR_PWM_TICK_HZ: u32 = 1_000_000;
