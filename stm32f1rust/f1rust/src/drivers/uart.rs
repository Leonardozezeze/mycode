//! 串口驱动：TX 入队后由 TXE 中断发送；RX 中断接收，留给主循环消费。
//! TX/RX 硬件句柄都由中断独占；UartLink 通过两个 SPSC 队列访问数据。

use crate::comm::ringbuf::{CAPACITY, Consumer, Producer, RingBuffer};
use core::sync::atomic::{AtomicU32, Ordering};
use core::{cell::UnsafeCell, fmt, marker::PhantomData};
use cortex_m::peripheral::NVIC;
use stm32f1xx_hal::{
    pac::{Interrupt, USART1, USART2, interrupt},
    serial::{Error, Instance, Rx, Tx},
};

/// 一次发送全部入队；空间不足时本次数据完全不入队。
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SendError {
    QueueFull,
}

/// 累计统计快照；各计数独立读取，计数器按 u32 回绕。
#[derive(Debug, Clone, Copy, Default)]
pub struct UartStats {
    pub rx_overrun: u32,
    pub rx_framing: u32,
    pub rx_parity: u32,
    pub rx_noise: u32,
    pub rx_other: u32,
    pub rx_dropped: u32,
    pub tx_queue_full: u32,
    pub tx_errors: u32,
}

struct Stats {
    rx_overrun: AtomicU32,
    rx_framing: AtomicU32,
    rx_parity: AtomicU32,
    rx_noise: AtomicU32,
    rx_other: AtomicU32,
    rx_dropped: AtomicU32,
    tx_queue_full: AtomicU32,
    tx_errors: AtomicU32,
}

impl Stats {
    const fn new() -> Self {
        Self {
            rx_overrun: AtomicU32::new(0),
            rx_framing: AtomicU32::new(0),
            rx_parity: AtomicU32::new(0),
            rx_noise: AtomicU32::new(0),
            rx_other: AtomicU32::new(0),
            rx_dropped: AtomicU32::new(0),
            tx_queue_full: AtomicU32::new(0),
            tx_errors: AtomicU32::new(0),
        }
    }

    fn rx_error(&self, error: Error) {
        let counter = match error {
            Error::Overrun => &self.rx_overrun,
            Error::FrameFormat => &self.rx_framing,
            Error::Parity => &self.rx_parity,
            Error::Noise => &self.rx_noise,
            _ => &self.rx_other,
        };
        counter.fetch_add(1, Ordering::Relaxed);
    }

    fn snapshot(&self) -> UartStats {
        UartStats {
            rx_overrun: self.rx_overrun.load(Ordering::Relaxed),
            rx_framing: self.rx_framing.load(Ordering::Relaxed),
            rx_parity: self.rx_parity.load(Ordering::Relaxed),
            rx_noise: self.rx_noise.load(Ordering::Relaxed),
            rx_other: self.rx_other.load(Ordering::Relaxed),
            rx_dropped: self.rx_dropped.load(Ordering::Relaxed),
            tx_queue_full: self.tx_queue_full.load(Ordering::Relaxed),
            tx_errors: self.tx_errors.load(Ordering::Relaxed),
        }
    }
}

struct IsrState<U: Instance> {
    tx: Tx<U>,
    rx: Rx<U>,
    // 先从队列取出的字节若遇到 WouldBlock，保留到下次中断，不丢失。
    pending_tx: Option<u8>,
    tx_consumer: Consumer<'static>,
    rx_producer: Producer<'static>,
}

struct IsrSlot<U: Instance>(UnsafeCell<Option<IsrState<U>>>);

// SAFETY: 初始化在临界区写入一次，之后只有本 USART 中断访问。
// UART 硬件句柄不暴露给主循环，同一中断不能抢占自身。
unsafe impl<U: Instance> Sync for IsrSlot<U> {}

pub struct UartLink<U: Instance> {
    tx_producer: Producer<'static>,
    rx_consumer: Consumer<'static>,
    stats: &'static Stats,
    irq: Interrupt,
    // Producer/Consumer 已经保证不可复制、不可共享引用跨上下文并发访问。
    _owner: PhantomData<U>,
}

impl<U: Instance> UartLink<U> {
    fn new(
        mut tx: Tx<U>,
        mut rx: Rx<U>,
        tx_buf: &'static mut RingBuffer,
        rx_buf: &'static mut RingBuffer,
        stats: &'static Stats,
        slot: &'static IsrSlot<U>,
        irq: Interrupt,
    ) -> Self {
        let (tx_producer, tx_consumer) = tx_buf.split();
        let (rx_producer, rx_consumer) = rx_buf.split();
        cortex_m::interrupt::free(|_| {
            // SAFETY: 临界区内中断不会访问 slot；只允许初始化一次。
            let state = unsafe { &mut *slot.0.get() };
            assert!(state.is_none(), "UART already initialized");
            tx.unlisten(); // 空队列不得开启 TXEIE，否则会产生持续中断。
            rx.listen();
            *state = Some(IsrState {
                tx,
                rx,
                pending_tx: None,
                tx_consumer,
                rx_producer,
            });
            unsafe { NVIC::unmask(irq) };
        });
        Self {
            tx_producer,
            rx_consumer,
            stats,
            irq,
            _owner: PhantomData,
        }
    }

    /// 非阻塞发送：复制进队列后立即返回，不等待线上发送完成。
    /// 最大单次长度为 CAPACITY - 1；空间不足时全部拒绝，由调用方决定重试。
    pub fn send(&mut self, data: &[u8]) -> Result<(), SendError> {
        if !self.tx_producer.try_push_slice(data) {
            self.stats.tx_queue_full.fetch_add(1, Ordering::Relaxed);
            return Err(SendError::QueueFull);
        }
        if !data.is_empty() {
            // 入队发布后再唤醒中断；主循环无需访问硬件，也不需要屏蔽中断。
            NVIC::pend(self.irq);
        }
        Ok(())
    }

    pub fn send_str(&mut self, s: &str) -> Result<(), SendError> {
        self.send(s.as_bytes())
    }

    /// 主循环读取，协议/Shell 解析由上层负责。
    pub fn read_byte(&mut self) -> Option<u8> {
        self.rx_consumer.pop()
    }

    pub fn stats(&self) -> UartStats {
        self.stats.snapshot()
    }
}

/// 整条格式化消息先写到栈上，成功后再一次入队，避免发出半条消息。
struct FormatBuffer {
    bytes: [u8; CAPACITY - 1],
    len: usize,
}

impl fmt::Write for FormatBuffer {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        if s.len() > self.bytes.len() - self.len {
            return Err(fmt::Error);
        }
        self.bytes[self.len..self.len + s.len()].copy_from_slice(s.as_bytes());
        self.len += s.len();
        Ok(())
    }
}

impl<U: Instance> fmt::Write for UartLink<U> {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        self.send_str(s).map_err(|_| fmt::Error)
    }

    fn write_fmt(&mut self, args: fmt::Arguments<'_>) -> fmt::Result {
        let mut buffer = FormatBuffer {
            bytes: [0; CAPACITY - 1],
            len: 0,
        };
        fmt::write(&mut buffer, args)?;
        self.send(&buffer.bytes[..buffer.len])
            .map_err(|_| fmt::Error)
    }
}

/// 单次中断的 RX 工作有界；错误已由 HAL read() 读 SR/DR 清除，记录后继续尝试。
fn service_rx<U: Instance>(state: &mut IsrState<U>, stats: &Stats) {
    for _ in 0..32 {
        match state.rx.read() {
            Ok(byte) => {
                if !state.rx_producer.push(byte) {
                    stats.rx_dropped.fetch_add(1, Ordering::Relaxed);
                }
            }
            Err(nb::Error::WouldBlock) => break,
            Err(nb::Error::Other(error)) => stats.rx_error(error),
        }
    }
}

/// 每次中断最多尝试发送一个字节，交给硬件继续发送，不在这里自旋等待。
fn service_tx<U: Instance>(state: &mut IsrState<U>, stats: &Stats) {
    if state.pending_tx.is_none() {
        state.pending_tx = state.tx_consumer.pop();
    }
    if let Some(byte) = state.pending_tx {
        match state.tx.write_u8(byte) {
            Ok(()) => state.pending_tx = None,
            Err(nb::Error::WouldBlock) => {}
            Err(nb::Error::Other(_)) => {
                stats.tx_errors.fetch_add(1, Ordering::Relaxed);
                state.pending_tx = None;
            }
        }
    }
    if state.pending_tx.is_some() || !state.tx_consumer.is_empty() {
        state.tx.listen();
    } else {
        state.tx.unlisten();
    }
}

// 每个 USART 独立的硬件泊位、队列与统计。
static STATS1: Stats = Stats::new();
static SLOT1: IsrSlot<USART1> = IsrSlot(UnsafeCell::new(None));

pub fn uart1(tx: Tx<USART1>, rx: Rx<USART1>) -> UartLink<USART1> {
    // singleton 只交出一次静态存储的独占引用，避免手写 static mut 访问。
    let tx_buf = cortex_m::singleton!(: RingBuffer = RingBuffer::new())
        .expect("USART1 TX buffer already taken");
    let rx_buf = cortex_m::singleton!(: RingBuffer = RingBuffer::new())
        .expect("USART1 RX buffer already taken");
    UartLink::new(tx, rx, tx_buf, rx_buf, &STATS1, &SLOT1, Interrupt::USART1)
}

#[interrupt]
fn USART1() {
    // SAFETY: 初始化后只有本中断访问 SLOT1；同一中断不会重入。
    if let Some(state) = unsafe { &mut *SLOT1.0.get() }.as_mut() {
        service_rx(state, &STATS1);
        service_tx(state, &STATS1);
    }
}

// USART2 使用独立队列、硬件泊位和统计，复用上面的收发处理逻辑。
static STATS2: Stats = Stats::new();
static SLOT2: IsrSlot<USART2> = IsrSlot(UnsafeCell::new(None));

pub fn uart2(tx: Tx<USART2>, rx: Rx<USART2>) -> UartLink<USART2> {
    let tx_buf = cortex_m::singleton!(: RingBuffer = RingBuffer::new())
        .expect("USART2 TX buffer already taken");
    let rx_buf = cortex_m::singleton!(: RingBuffer = RingBuffer::new())
        .expect("USART2 RX buffer already taken");
    UartLink::new(tx, rx, tx_buf, rx_buf, &STATS2, &SLOT2, Interrupt::USART2)
}

#[interrupt]
fn USART2() {
    // SAFETY: 初始化后只有本中断访问 SLOT2；同一中断不会重入。
    if let Some(state) = unsafe { &mut *SLOT2.0.get() }.as_mut() {
        service_rx(state, &STATS2);
        service_tx(state, &STATS2);
    }
}
