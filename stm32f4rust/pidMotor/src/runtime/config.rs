//! 电机参数与运行期任务配置；驱动层不依赖本模块。

use crate::comm::proto::ByteOrder;

/// 电机 PWM 频率，单位 Hz。
pub const MOTOR_PWM_HZ: u32 = 1_000;
/// 编码器线数：电机轴每圈的单相脉冲数。
pub const ENCODER_PPR: f32 = 11.0;
/// 对应 HAL 固定使用的编码器模式 3；此参数只用于换算，不配置硬件。
pub const ENCODER_MULTIPLIER: f32 = 4.0;
/// 电机轴转数 / 输出轴转数。
pub const GEAR_RATIO: f32 = 21.3;
/// 输出轴每圈计数：11 × 4 × 21.3 = 937.2。
pub const COUNTS_PER_REV: f32 = ENCODER_PPR * ENCODER_MULTIPLIER * GEAR_RATIO;
/// 测速采样周期，单位 ms，必须大于零。
pub const SPEED_SAMPLE_PERIOD_MS: u64 = 10;

/// 破碎率映射范围，低破碎率对应高目标转速。
pub const B_MIN: f32 = 2.0;
pub const B_MAX: f32 = 5.0;
pub const RPM_MIN: f32 = 50.0;
pub const RPM_MAX: f32 = 250.0;

pub const PID_KP: f32 = 0.3;
/// 旧代码每 10 ms 使用 Ki=0.2；本项目按秒计算积分，等效 Ki=0.2/0.01。
pub const PID_KI: f32 = 20.0;
pub const PID_KD: f32 = 0.0;
/// 保留旧业务的归一化 PWM 单位：1000 表示满占空比。
pub const PID_OUTPUT_MAX: f32 = 1_000.0;
pub const HEARTBEAT_PERIOD_MS: u64 = 1_000;

/// 主循环每轮最多处理的传感器字节数，避免连续接收占用整个循环。
pub const SENSOR_RX_BUDGET: usize = 64;
/// 传感器的两个 float 均为 4 字节 IEEE 754 单精度，采用小端字节序。
pub const SENSOR_BYTE_ORDER: ByteOrder = ByteOrder::LittleEndian;
