//! Embassy UART：DMA 异步发送，DMA 环形缓冲持续接收。
//! TX/RX 可拆分给独立任务，错误统计通过原子计数器共享。

use core::sync::atomic::{AtomicU32, Ordering};
use embassy_stm32::{
    mode::Async,
    usart::{Error, RingBufferedUartRx, Uart, UartTx},
};

/// HAL 报告的错误事件次数，不是硬件上每个错误字节的精确计数。
/// 各字段独立读取；RX 溢出不能据此推算丢失字节数。
#[derive(Debug, Clone, Copy, Default)]
pub struct UartStats {
    pub rx_overrun: u32,
    pub rx_framing: u32,
    pub rx_parity: u32,
    pub rx_noise: u32,
    pub rx_other: u32,
    pub tx_errors: u32,
}

/// 每个串口实例使用一份静态计数器，不与硬件句柄共用可变引用。
pub struct UartCounters {
    rx_overrun: AtomicU32,
    rx_framing: AtomicU32,
    rx_parity: AtomicU32,
    rx_noise: AtomicU32,
    rx_other: AtomicU32,
    tx_errors: AtomicU32,
}

impl UartCounters {
    pub const fn new() -> Self {
        Self {
            rx_overrun: AtomicU32::new(0),
            rx_framing: AtomicU32::new(0),
            rx_parity: AtomicU32::new(0),
            rx_noise: AtomicU32::new(0),
            rx_other: AtomicU32::new(0),
            tx_errors: AtomicU32::new(0),
        }
    }

    pub fn snapshot(&self) -> UartStats {
        UartStats {
            rx_overrun: self.rx_overrun.load(Ordering::Relaxed),
            rx_framing: self.rx_framing.load(Ordering::Relaxed),
            rx_parity: self.rx_parity.load(Ordering::Relaxed),
            rx_noise: self.rx_noise.load(Ordering::Relaxed),
            rx_other: self.rx_other.load(Ordering::Relaxed),
            tx_errors: self.tx_errors.load(Ordering::Relaxed),
        }
    }

    fn record_rx_error(&self, error: Error) {
        let counter = match error {
            Error::Overrun => &self.rx_overrun,
            Error::Framing => &self.rx_framing,
            Error::Parity => &self.rx_parity,
            Error::Noise => &self.rx_noise,
            _ => &self.rx_other,
        };
        counter.fetch_add(1, Ordering::Relaxed);
    }
}

pub struct UartLink {
    tx: UartSender,
    rx: UartReceiver,
}

pub struct UartSender {
    tx: UartTx<'static, Async>,
    counters: &'static UartCounters,
}

pub struct UartReceiver {
    rx: RingBufferedUartRx<'static>,
    counters: &'static UartCounters,
}

impl UartLink {
    pub fn new(
        uart: Uart<'static, Async>,
        rx_buffer: &'static mut [u8],
        counters: &'static UartCounters,
    ) -> Self {
        let (tx, rx) = uart.split();
        let mut rx = rx.into_ring_buffered(rx_buffer);
        rx.start_uart();
        Self {
            tx: UartSender { tx, counters },
            rx: UartReceiver { rx, counters },
        }
    }

    /// 所有权拆分：每个方向只能交给一个任务，不用 Mutex 锁住整个串口。
    pub fn split(self) -> (UartSender, UartReceiver) {
        (self.tx, self.rx)
    }
}

impl UartSender {
    /// 等待 DMA 完成；最后一个字节在线上发完需再 flush。
    /// 取消 future 时可能已发出部分数据，不能保证整条消息撤回。
    pub async fn send(&mut self, data: &[u8]) -> Result<(), Error> {
        // F4 DMA 计数为 16 位；空消息直接成功，长消息分段发送。
        for chunk in data.chunks(u16::MAX as usize) {
            if let Err(error) = self.tx.write(chunk).await {
                self.counters.tx_errors.fetch_add(1, Ordering::Relaxed);
                return Err(error);
            }
        }
        Ok(())
    }

    pub async fn send_str(&mut self, text: &str) -> Result<(), Error> {
        self.send(text.as_bytes()).await
    }

    pub async fn flush(&mut self) -> Result<(), Error> {
        self.tx.flush().await
    }

    pub fn stats(&self) -> UartStats {
        self.counters.snapshot()
    }
}

impl UartReceiver {
    /// 等数据到达；错误后 HAL 停止接收，下一次 read 自动重启。
    pub async fn read(&mut self, data: &mut [u8]) -> Result<usize, Error> {
        if data.is_empty() {
            return Ok(0);
        }
        match self.rx.read(data).await {
            Ok(len) => Ok(len),
            Err(error) => {
                self.counters.record_rx_error(error);
                Err(error)
            }
        }
    }

    pub async fn read_byte(&mut self) -> Result<u8, Error> {
        let mut byte = [0];
        self.read(&mut byte).await?;
        Ok(byte[0])
    }

    pub fn stats(&self) -> UartStats {
        self.counters.snapshot()
    }
}
