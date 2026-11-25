pub const PID_KP: f32 = 0.3;
pub const PID_KI: f32 = 20.0;
pub const PID_KD: f32 = 0.0;
/// 保留旧业务的归一化 PWM 单位：1000 表示满占空比。
pub const PID_OUTPUT_MAX: f32 = 1_000.0;
pub const HEARTBEAT_PERIOD_MS: u64 = 400;

/// USART1 波特率，串口数据格式采用 8N1。
pub const UART1_BAUD_RATE: u32 = 115_200;
/// USART1 DMA 环形接收缓冲区大小，单位为字节。
pub const UART1_RX_DMA_BUFFER_BYTES: usize = 256;

/// UART 后台收发任务各自允许同时运行的串口数量。
pub const UART_TASK_POOL_SIZE: usize = 1;

/// 每个串口独立分配队列；按整条消息计数，容量固定，不使用堆分配。
pub const UART_TX_QUEUE_CAPACITY: usize = 8;
pub const UART_TX_MESSAGE_BYTES: usize = 128;
pub const UART_RX_QUEUE_CAPACITY: usize = 4;
pub const UART_RX_CHUNK_BYTES: usize = 64;

/// CAN1 总线位速率，单位为 bit/s。
pub const CAN1_BITRATE: u32 = 500_000;
/// CAN 发送队列容量，按帧计数。
pub const CAN_TX_QUEUE_CAPACITY: usize = 4;
/// CAN 接收队列容量，帧和总线错误事件各占一个槽位。
pub const CAN_RX_QUEUE_CAPACITY: usize = 8;

const _: () = assert!(
    UART1_RX_DMA_BUFFER_BYTES > 0 && UART1_RX_DMA_BUFFER_BYTES <= u16::MAX as usize,
    "UART1_RX_DMA_BUFFER_BYTES must be in 1..=65535",
);
const _: () = assert!(
    CAN_TX_QUEUE_CAPACITY > 0 && CAN_RX_QUEUE_CAPACITY > 0,
    "CAN queue capacities must be greater than zero",
);
