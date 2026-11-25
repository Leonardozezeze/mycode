//! Embassy UART：DMA 异步发送，DMA 环形缓冲持续接收。
//! 业务直接共享 UartLink，硬件由后台收发任务独占。

use crate::runtime::config;
use core::{
    cell::Cell,
    fmt::{self, Write},
};
use embassy_stm32::usart::Error;
use embassy_sync::{
    blocking_mutex::{Mutex, raw::CriticalSectionRawMutex},
    channel::{Channel, TrySendError},
};
use heapless::Vec;

/// 创建独立的静态 DMA 缓冲和串口实例，启动后台任务，返回 &'static UartLink。
/// 缓冲区大小必须是编译期常量；同一调用位置只能执行一次，重复调用会 panic。
macro_rules! init_uart {
    ($spawner:expr, $uart:expr, $rx_buffer_bytes:expr $(,)?) => {{
        let spawner = $spawner;
        let uart = $uart;
        static RX_BUFFER: ::static_cell::StaticCell<[u8; $rx_buffer_bytes]> =
            ::static_cell::StaticCell::new();
        static LINK: ::static_cell::StaticCell<$crate::drivers::uart::UartLink> =
            ::static_cell::StaticCell::new();

        $crate::runtime::tasks::uart_task::start(
            spawner,
            uart,
            RX_BUFFER.init_with(|| [0; $rx_buffer_bytes]),
            LINK.init_with($crate::drivers::uart::UartLink::new),
        )
    }};
}

pub(crate) use init_uart;

/// 格式化文本并非阻塞入队，返回 Result<(), TrySendFmtError>，不自动添加换行。
/// 例如：uart_print!(uart1, "tick {}ms\r\n", milliseconds)。
#[allow(unused)]
macro_rules! uart_print {
    ($uart:expr, $($args:tt)*) => {
        ($uart).try_send_fmt(::core::format_args!($($args)*))
    };
}
#[allow(unused)]
pub(crate) use uart_print;

pub type TxMessage = Vec<u8, { config::UART_TX_MESSAGE_BYTES }>;
pub type RxChunk = Vec<u8, { config::UART_RX_CHUNK_BYTES }>;

/// 二进制数据超过 UART_TX_MESSAGE_BYTES，整条消息未入队。
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct MessageTooLong;

/// 格式化日志入队失败的原因，不包含 UART 硬件发送错误。
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TrySendFmtError {
    /// 文本超过单条消息容量，或格式化实现返回错误；整条消息不入队。
    Format,
    QueueFull,
}

/// 数据块不是协议帧；错误事件代表接收流出现断点，后续协议解析应重新同步。
pub enum RxEvent {
    Data(RxChunk),
    Error(Error),
}

/// 串口统计快照。RX 计数反映 HAL 上报的事件，溢出次数无法推算丢失字节数。
#[derive(Debug, Clone, Copy, Default)]
pub struct UartStats {
    pub rx_error_events: u32,
    pub last_rx_error: Option<&'static str>,
    pub rx_overrun: u32,
    pub rx_framing: u32,
    pub rx_parity: u32,
    pub rx_noise: u32,
    pub rx_other: u32,
    pub tx_errors: u32,
    /// 软件发送队列拒绝非阻塞发送的次数。
    pub tx_queue_full: u32,
}

/// 初始化后直接将共享引用传给业务任务，队列和计数器始终属于同一个串口。
pub struct UartLink {
    // 仅供后台收发任务取发送消息、交付接收事件；业务使用下方的方法。
    pub(crate) tx_queue:
        Channel<CriticalSectionRawMutex, TxMessage, { config::UART_TX_QUEUE_CAPACITY }>,
    pub(crate) rx_queue:
        Channel<CriticalSectionRawMutex, RxEvent, { config::UART_RX_QUEUE_CAPACITY }>,
    stats: Mutex<CriticalSectionRawMutex, Cell<UartStats>>,
}

impl UartLink {
    /// 由初始化宏创建，业务通过 init_uart! 获得已启动的串口。
    pub(crate) const fn new() -> Self {
        Self {
            tx_queue: Channel::new(),
            rx_queue: Channel::new(),
            stats: Mutex::new(Cell::new(UartStats {
                rx_error_events: 0,
                last_rx_error: None,
                rx_overrun: 0,
                rx_framing: 0,
                rx_parity: 0,
                rx_noise: 0,
                rx_other: 0,
                tx_errors: 0,
                tx_queue_full: 0,
            })),
        }
    }

    /// 等待发送队列有空间后整条入队；返回时不代表 DMA 已发送完成。
    pub async fn send(&self, message: TxMessage) {
        self.tx_queue.send(message).await;
    }

    /// 将原始字节复制为一条消息并等待入队，不格式化、不添加换行。
    /// 超过 UART_TX_MESSAGE_BYTES 时返回错误；成功只表示已入队。
    pub async fn send_bytes(&self, data: &[u8]) -> Result<(), MessageTooLong> {
        let message = TxMessage::from_slice(data).map_err(|_| MessageTooLong)?;
        self.send(message).await;
        Ok(())
    }

    /// 非阻塞地整条入队；队列满时通过错误返回原消息。
    pub fn try_send(&self, message: TxMessage) -> Result<(), TrySendError<TxMessage>> {
        let result = self.tx_queue.try_send(message);
        if result.is_err() {
            self.stats.lock(|cell| {
                let mut stats = cell.get();
                stats.tx_queue_full = stats.tx_queue_full.saturating_add(1);
                cell.set(stats);
            });
        }
        result
    }

    /// 格式化后非阻塞地整条入队，不使用堆分配；失败时不会发送部分文本。
    /// 不自动添加换行，成功返回只表示已入队，不代表 DMA 已发送完成。
    /// 业务可以用 uart_print! 宏省略 format_args!。
    pub fn try_send_fmt(&self, args: fmt::Arguments<'_>) -> Result<(), TrySendFmtError> {
        let mut message = TxMessage::new();
        message
            .write_fmt(args)
            .map_err(|_| TrySendFmtError::Format)?;
        self.try_send(message)
            .map_err(|_| TrySendFmtError::QueueFull)
    }

    /// 等待下一条接收数据或错误事件，应由一个业务任务统一消费。
    /// 多个接收者会分走事件，不会广播。
    pub async fn receive(&self) -> RxEvent {
        self.rx_queue.receive().await
    }

    pub fn stats(&self) -> UartStats {
        self.stats.lock(Cell::get)
    }

    pub(crate) fn record_tx_error(&self) {
        self.stats.lock(|cell| {
            let mut stats = cell.get();
            stats.tx_errors = stats.tx_errors.saturating_add(1);
            cell.set(stats);
        });
    }

    pub(crate) fn record_rx_error(&self, error: Error) {
        self.stats.lock(|cell| {
            let mut stats = cell.get();
            let (name, count) = match error {
                Error::Overrun => ("overrun", &mut stats.rx_overrun),
                Error::Framing => ("framing", &mut stats.rx_framing),
                Error::Parity => ("parity", &mut stats.rx_parity),
                Error::Noise => ("noise", &mut stats.rx_noise),
                _ => ("other", &mut stats.rx_other),
            };
            *count = count.saturating_add(1);
            stats.rx_error_events = stats.rx_error_events.saturating_add(1);
            stats.last_rx_error = Some(name);
            cell.set(stats);
        });
    }
}
