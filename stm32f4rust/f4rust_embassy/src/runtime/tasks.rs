//! 异步任务和消息通道。硬件句柄只由对应任务独占。

use crate::drivers::uart::{UartReceiver, UartSender};
use crate::runtime::config;
use core::{
    fmt::Write,
    sync::atomic::{AtomicU32, Ordering},
};
use embassy_stm32::{gpio::Output, usart::Error};
use embassy_sync::{
    blocking_mutex::raw::CriticalSectionRawMutex,
    channel::{Channel, Receiver, Sender},
};
use embassy_time::{Duration, Instant, Ticker};
use heapless::{String, Vec};

pub type TxMessage = Vec<u8, { config::UART_TX_MESSAGE_BYTES }>;
pub type RxChunk = Vec<u8, { config::UART_RX_CHUNK_BYTES }>;

/// 数据块不是协议帧；错误事件代表接收流出现断点，后续协议解析应重新同步。
pub enum RxEvent {
    Data(RxChunk),
    Error(Error),
}

pub type TxQueue = Channel<CriticalSectionRawMutex, TxMessage, { config::UART_TX_QUEUE_CAPACITY }>;
pub type RxQueue = Channel<CriticalSectionRawMutex, RxEvent, { config::UART_RX_QUEUE_CAPACITY }>;
type TxSender =
    Sender<'static, CriticalSectionRawMutex, TxMessage, { config::UART_TX_QUEUE_CAPACITY }>;
type TxReceiver =
    Receiver<'static, CriticalSectionRawMutex, TxMessage, { config::UART_TX_QUEUE_CAPACITY }>;
type RxSender =
    Sender<'static, CriticalSectionRawMutex, RxEvent, { config::UART_RX_QUEUE_CAPACITY }>;

/// 心跳日志入队失败的累计次数，按 u32 回绕；不包含 UART 硬件错误。
static HEARTBEAT_LOG_DROPPED: AtomicU32 = AtomicU32::new(0);

pub fn heartbeat_log_dropped() -> u32 {
    HEARTBEAT_LOG_DROPPED.load(Ordering::Relaxed)
}

#[embassy_executor::task]
pub async fn heartbeat_task(mut led: Output<'static>, tx: TxSender) {
    let mut ticker = Ticker::every(Duration::from_millis(config::HEARTBEAT_PERIOD_MS));
    loop {
        ticker.next().await;
        led.toggle();
        let mut text = String::<64>::new();
        write!(text, "tick {}ms\r\n", Instant::now().as_millis())
            .expect("heartbeat message too long");
        let message =
            TxMessage::from_slice(text.as_bytes()).expect("TX message capacity too small");
        // 日志拥塞不能延迟下一次闪灯；整条拒绝，不会只入队半条消息。
        if tx.try_send(message).is_err() {
            HEARTBEAT_LOG_DROPPED.fetch_add(1, Ordering::Relaxed);
        }
    }
}

#[embassy_executor::task]
pub async fn uart_tx_task(mut uart: UartSender, messages: TxReceiver) {
    loop {
        // 局部 message 在 DMA 完成前一直有效；本任务串行发送，消息不会交错。
        let message = messages.receive().await;
        // HAL 错误已计数；不自动重发，避免部分发送后产生重复协议数据。
        let _ = uart.send(&message).await;
    }
}

#[embassy_executor::task]
pub async fn uart_rx_task(mut uart: UartReceiver, events: RxSender) {
    let mut buffer = [0; config::UART_RX_CHUNK_BYTES];
    loop {
        match uart.read(&mut buffer).await {
            Ok(0) => embassy_futures::yield_now().await,
            Ok(len) => {
                let data =
                    RxChunk::from_slice(&buffer[..len]).expect("RX chunk capacity too small");
                // 保序交付，队列满时等待；应用必须及时处理，否则 DMA 环形缓冲仍可能溢出。
                events.send(RxEvent::Data(data)).await;
            }
            Err(error) => {
                events.send(RxEvent::Error(error)).await;
                // 连续错误时也给其他就绪任务执行机会；下次 read 自动恢复接收。
                embassy_futures::yield_now().await;
            }
        }
    }
}
