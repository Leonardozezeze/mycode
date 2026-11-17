use crate::runtime::config;
use heapless::Vec;
use embassy_sync::{
    blocking_mutex::raw::CriticalSectionRawMutex,
    channel::Channel,
};
pub type TxMessage=Vec<u8,{config::UART_TX_MESSAGE_BYTES}>;//定义消息发送缓冲区类型
pub type TxQueue =
    Channel<CriticalSectionRawMutex, TxMessage, { config::UART_TX_QUEUE_CAPACITY }>;
pub type TxReceiver = embassy_sync::channel::Receiver<
    'static,
    CriticalSectionRawMutex,
    TxMessage,
    { config::UART_TX_QUEUE_CAPACITY },
>;
pub type RxChunk = Vec<u8, { config::UART_RX_CHUNK_BYTES }>;
pub enum RxEvent {
    Data(RxChunk),
    Error(embassy_stm32::usart::Error),
}
pub type RxQueue = Channel<
    CriticalSectionRawMutex,
    RxEvent,
    { config::UART_RX_QUEUE_CAPACITY },
>;
pub type RxSender = embassy_sync::channel::Sender<
    'static,
    CriticalSectionRawMutex,
    RxEvent,
    { config::UART_RX_QUEUE_CAPACITY },
>;
#[embassy_executor::task]
pub async fn uart_tx_task(
    mut tx: embassy_stm32::usart::UartTx<'static, embassy_stm32::mode::Async>,
    messages: TxReceiver,
) {
    loop {
        let message = messages.receive().await;
        tx.write(&message).await.expect("UART TX failed");
    }
}
#[embassy_executor::task]
pub async fn uart_rx_task(
    mut rx: embassy_stm32::usart::RingBufferedUartRx<'static>,
    events: RxSender,
) {
    let mut buffer = [0u8; config::UART_RX_CHUNK_BYTES];

    loop {
        match rx.read(&mut buffer).await {
            Ok(0) => {
                embassy_futures::yield_now().await;
            }
            Ok(n) => {
                let data = RxChunk::from_slice(&buffer[..n])
                    .expect("RX chunk capacity too small");

                events.send(RxEvent::Data(data)).await;
            }
            Err(error) => {
                events.send(RxEvent::Error(error)).await;
                embassy_futures::yield_now().await;
            }
        }
    }
}