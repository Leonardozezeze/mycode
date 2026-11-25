pub const PID_KP: f32 = 0.3;
pub const PID_KI: f32 = 20.0;
pub const PID_KD: f32 = 0.0;
/// 保留旧业务的归一化 PWM 单位：1000 表示满占空比。
pub const PID_OUTPUT_MAX: f32 = 1_000.0;
pub const HEARTBEAT_PERIOD_MS: u64 = 400;
/// 虚拟传感器更新并发送一帧的周期：100 ms，即 10 Hz。
pub const VIRTUAL_SENSOR_PERIOD_MS: u64 = 100;

/// UART 后台收发任务各自允许同时运行的串口数量。
pub const UART_TASK_POOL_SIZE: usize = 2;

/// 每个串口独立分配队列；按整条消息计数，容量固定，不使用堆分配。
pub const UART_TX_QUEUE_CAPACITY: usize = 8;
pub const UART_TX_MESSAGE_BYTES: usize = 128;
pub const UART_RX_QUEUE_CAPACITY: usize = 4;
pub const UART_RX_CHUNK_BYTES: usize = 64;

//定义虚拟的最大破碎含杂率和最小破碎含杂率
pub const MAX_BROKEN_RATE: f32 = 0.033;
pub const MIN_BROKEN_RATE: f32 = 0.015;
pub const MAX_IMPURITY_RATE: f32 = 0.040;
pub const MIN_IMPURITY_RATE: f32 = 0.020;
pub const MAX_MOTOR_SPEED: f32 = 300.0;
pub const MIN_MOTOR_SPEED: f32 = 50.0;

/// 小于此位移变化时保留当前目标，减少继电器切换；端点目标不受此死区限制。
pub const ACTUATOR_DEADBAND_MM: f32 = 0.5;

/// 推杆根据每个窗口内的平均含杂率更新目标，单位 ms。
pub const ACTUATOR_AVERAGE_WINDOW_MS: u64 = 3_000;

/// 短时演示配置：回零及每次动作后释放输出并等待 1 秒。
/// 此模式不执行长期 10% 占空比限制。
pub const ACTUATOR_REST_MS: u64 = 1000;
/// 推杆演示任务总时长（包含回零），到期停止，其他任务继续运行。
pub const ACTUATOR_DEMO_SECONDS: u64 = 120;
