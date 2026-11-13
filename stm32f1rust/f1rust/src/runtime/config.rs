pub const PID_KP: f32 = 0.3;
pub const PID_KI: f32 = 20.0;
pub const PID_KD: f32 = 0.0;
/// 保留旧业务的归一化 PWM 单位：1000 表示满占空比。
pub const PID_OUTPUT_MAX: f32 = 1_000.0;
pub const HEARTBEAT_PERIOD_MS: u64 = 500;
