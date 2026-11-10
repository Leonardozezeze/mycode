//! 串口器件驱动：TX 阻塞发送；RX 走中断 + comm::ringbuf（契约 2）。
//!
//! 泛型核心（IsrRx / UartLink）对任意 USART 实例复用；
//! 每实例胶水（statics + 构造器 + 中断处理函数）目前只有 USART1 一份，
//! 加第二个串口时照抄一份或宏化。

use crate::comm::ringbuf::RingBuffer;
use core::cell::UnsafeCell;
use core::fmt;
use cortex_m::peripheral::NVIC;
use stm32f4xx_hal::{
    nb::block,
    // pac::interrupt 一次带入两样东西（PAC 双命名空间导出）：
    // 属性宏 #[interrupt]，以及枚举别名 interrupt = Interrupt
    // （宏展开会注入 `interrupt::USART1;` 编译期校验中断名，需要后者才能解析）
    pac::{Interrupt, USART1, interrupt},
    prelude::*, // 带入 RxListen 和 embedded-hal-nb 的 Read
    serial::{Instance, Rx, Tx},
};

/// Rx 句柄的泊位：UartLink::new 时写入一次，之后只有中断处理函数访问。
struct IsrRx<U: Instance>(UnsafeCell<Option<Rx<U>>>);

// SAFETY: 唯一一次写发生在 UartLink::new（主线程，此时中断还未 unmask）；
// 之后只有本实例的中断处理函数取 &mut，主循环从不触碰——任意时刻单写者。
unsafe impl<U: Instance> Sync for IsrRx<U> {}

impl<U: Instance> IsrRx<U> {
    /// 泊入句柄（只许在 NVIC unmask 之前调用）
    pub(crate) unsafe fn park(&self, rx: Rx<U>) {
        // edition 2024：unsafe fn 的函数体不再隐式是 unsafe 块，
        // 解引用裸指针仍须显式包裹
        unsafe { *self.0.get() = Some(rx) };
    }
    /// 中断处理函数专用访问
    pub(crate) unsafe fn with_rx<R>(&self, f: impl FnOnce(&mut Rx<U>) -> R) -> Option<R> {
        unsafe { (*self.0.get()).as_mut().map(f) }
    }
}

pub struct UartLink<U: Instance> {
    tx: Tx<U>,
    rx_buf: &'static RingBuffer, // 每个实例自己的池，不共享
}

impl<U: Instance> UartLink<U> {
    /// 组装 TX，并打通 RX 中断路径：泊入 Rx → 开 RXNEIE → 开 NVIC。
    /// 模块私有装配入口；对外只有每实例构造器（如 [`uart1`]），
    /// 它把私有的 IsrRx/池/中断号都绑定好了。
    fn new(
        tx: Tx<U>,
        rx: Rx<U>,
        rx_buf: &'static RingBuffer,
        rx_slot: &'static IsrRx<U>,
        irq: Interrupt,
    ) -> Self {
        unsafe {
            rx_slot.park(rx);
            let _ = rx_slot.with_rx(|r| r.listen()); // 先泊入、再开外设位
            NVIC::unmask(irq); // 最后放行 NVIC——顺序即合同
        }
        Self { tx, rx_buf }
    }

    /// 阻塞发送字节序列
    pub fn send(&mut self, data: &[u8]) {
        for &b in data {
            // write 只会在 TXE 未就绪时返回 WouldBlock，block! 原地等；
            // 发送方向没有其他错误来源，unwrap 可接受
            block!(self.tx.write(b)).unwrap();
        }
        block!(self.tx.flush()).unwrap();
    }

    pub fn send_str(&mut self, s: &str) {
        self.send(s.as_bytes());
    }

    /// 主循环消费接收缓冲（之后直接喂 comm::proto 解析器）
    pub fn read_byte(&self) -> Option<u8> {
        self.rx_buf.pop()
    }
}

impl<U: Instance> fmt::Write for UartLink<U> {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        self.send(s.as_bytes());
        Ok(()) // 我们的 send 不会失败
    }
}

// —— 每实例胶水：statics + 构造器 + 中断处理函数 ——

/// USART1 的 RX 字节池：中断生产（push），主循环消费（pop）
static RX_BUF1: RingBuffer = RingBuffer::new();
static RX_SLOT1: IsrRx<USART1> = IsrRx(UnsafeCell::new(None));
/// USART1 的对外构造入口：绑好它的池、泊位和中断号
pub fn uart1(tx: Tx<USART1>, rx: Rx<USART1>) -> UartLink<USART1> {
    UartLink::new(tx, rx, &RX_BUF1, &RX_SLOT1, Interrupt::USART1)
}

/// USART1 中断：把已到达的字节压入环形缓冲。保持短小——执行期间主循环停摆。
#[interrupt]
fn USART1() {
    // 单次中断最多清 32 字节；没清完的话 RXNE 仍置位，
    // 退出中断后立即重入，天然续传，ISR 耗时却有上界
    for _ in 0..32 {
        match unsafe { RX_SLOT1.with_rx(|rx| rx.read().ok()).flatten() } {
            Some(b) => {
                RX_BUF1.push(b); // 池满丢字节，ringbuf 的既定契约
            }
            None => break, // WouldBlock（没字节了）或线路错误
        }
    }
}
