// rust/src/config.rs
//! 电机/编码器硬件参数，换电机只改这里

/// 编码器线数（电机轴每圈脉冲）
pub const ENCODER_PPR: f32 = 11.0;
/// TIM4 TI12 模式 = 四倍频
pub const ENCODER_MULTIPLIER: f32 = 4.0;
/// 减速比
pub const GEAR_RATIO: f32 = 21.3;

/// 输出轴转一圈的计数 = 11 × 4 × 21.3 = 937.2
pub const COUNTS_PER_REV: f32 = ENCODER_PPR * ENCODER_MULTIPLIER * GEAR_RATIO;
