//! 输出轴测速的换算、采样和滤波参数。

/// 编码器线数，单位为电机轴每圈的单相脉冲数。
pub const ENCODER_PPR: f32 = 11.0;
/// 对应 TIM4 编码器模式 3 的四倍频计数。
pub const ENCODER_MULTIPLIER: f32 = 4.0;
/// 电机轴转数与输出轴转数之比。
pub const GEAR_RATIO: f32 = 21.3;
/// 输出轴每圈计数为 11 × 4 × 21.3 = 937.2。
pub const COUNTS_PER_REV: f32 = ENCODER_PPR * ENCODER_MULTIPLIER * GEAR_RATIO;

/// 测速采样周期，单位 ms，必须大于零。
pub const SPEED_SAMPLE_PERIOD_MS: u64 = 10;
/// 测速一阶低通的时间常数，单位 ms。设为 0 时关闭滤波。
pub const SPEED_FILTER_TIME_CONSTANT_MS: u64 = 30;

/// 沿用旧工程的输出轴最高目标转速，单位 RPM。
pub const SPEED_TARGET_MAX_RPM: f32 = 250.0;

const _: () = assert!(SPEED_SAMPLE_PERIOD_MS > 0);
