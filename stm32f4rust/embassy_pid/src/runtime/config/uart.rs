//! UART 波特率、DMA 缓冲和收发队列参数。

/// USART1 波特率，串口数据格式采用 8N1。
pub const UART1_BAUD_RATE: u32 = 115_200;
/// USART1 DMA 环形接收缓冲区大小，单位为字节。
pub const UART1_RX_DMA_BUFFER_BYTES: usize = 256;

/// UART 后台收发任务各自允许同时运行的串口数量。
pub const UART_TASK_POOL_SIZE: usize = 1;

/// 每个串口独立分配固定容量的队列，容量按整条消息计数。
pub const UART_TX_QUEUE_CAPACITY: usize = 8;
pub const UART_TX_MESSAGE_BYTES: usize = 128;
pub const UART_RX_QUEUE_CAPACITY: usize = 4;
pub const UART_RX_CHUNK_BYTES: usize = 64;

const _: () = assert!(
    UART1_RX_DMA_BUFFER_BYTES > 0 && UART1_RX_DMA_BUFFER_BYTES <= u16::MAX as usize,
    "UART1_RX_DMA_BUFFER_BYTES must be in 1..=65535",
);
