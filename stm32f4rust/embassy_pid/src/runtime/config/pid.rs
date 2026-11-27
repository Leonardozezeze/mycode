//! PID 增益和输出限幅。

pub const PID_KP: f32 = 0.3;
pub const PID_KI: f32 = 20.0;
pub const PID_KD: f32 = 0.0;
/// 在线调参可设置的单项增益上限。
pub const PID_GAIN_MAX: f32 = 1_000.0;
/// 归一化 PWM 指令的幅值上限，1000 表示满占空比。
pub const PID_OUTPUT_MAX: f32 = 1_000.0;
