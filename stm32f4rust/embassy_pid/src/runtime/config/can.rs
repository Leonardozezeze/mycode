//! CAN 总线和收发队列参数。

/// CAN1 总线位速率，单位为 bit/s。
pub const CAN1_BITRATE: u32 = 500_000;
/// CAN 发送队列容量，按帧计数。
pub const CAN_TX_QUEUE_CAPACITY: usize = 4;
/// CAN 接收队列容量，帧和总线错误事件各占一个槽位。
pub const CAN_RX_QUEUE_CAPACITY: usize = 8;
/// 电机转速遥测周期，20 ms 对应 50 Hz。
pub const CAN_SPEED_TELEMETRY_PERIOD_MS: u64 = 20;

const _: () = assert!(
    CAN_TX_QUEUE_CAPACITY > 0 && CAN_RX_QUEUE_CAPACITY > 0,
    "CAN queue capacities must be greater than zero",
);
const _: () = assert!(CAN_SPEED_TELEMETRY_PERIOD_MS > 0);
