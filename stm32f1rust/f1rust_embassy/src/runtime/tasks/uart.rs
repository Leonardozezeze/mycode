//! 串口收发任务：使用串口初始化时绑定的消息队列。
use crate::drivers::uart::{RxChunk, RxEvent, UartLink};
use crate::runtime::config;
use embassy_executor::Spawner;
use embassy_stm32::{
    mode::Async,
    usart::{RingBufferedUartRx, Uart, UartTx},
};

/// 由初始化宏调用，硬件拆分仅发生在此处，业务直接共享串口实例。
/// 当前收发任务各有一个执行槽；槽已占用时 panic。
pub(crate) fn start(
    spawner: Spawner,
    uart: Uart<'static, Async>,
    rx_buffer: &'static mut [u8],
    link: &'static mut UartLink,
) -> &'static UartLink {
    // 独占借用确保同一个实例只能绑定给一个串口，启动后只共享队列。
    let link: &'static UartLink = link;
    let (tx, rx) = uart.split();
    let mut rx = rx.into_ring_buffered(rx_buffer);
    rx.start_uart();
    spawner
        .spawn(uart_tx_task(tx, link))
        .expect("failed to spawn UART TX task");
    spawner
        .spawn(uart_rx_task(rx, link))
        .expect("failed to spawn UART RX task");
    link
}

#[embassy_executor::task]
async fn uart_tx_task(mut tx: UartTx<'static, Async>, uart: &'static UartLink) {
    loop {
        // 局部 message 在 DMA 完成前一直有效；本任务串行发送，消息不会交错。
        let message = uart.tx_queue.receive().await;
        // STM32 DMA 计数为 16 位；空消息无需发送，长消息分段发送。
        for chunk in message.chunks(u16::MAX as usize) {
            if tx.write(chunk).await.is_err() {
                uart.record_tx_error();
                // 不自动重发，避免部分发送后产生重复协议数据。
                break;
            }
        }
    }
}

#[embassy_executor::task]
async fn uart_rx_task(mut rx: RingBufferedUartRx<'static>, uart: &'static UartLink) {
    let mut buffer = [0; config::UART_RX_CHUNK_BYTES];
    loop {
        match rx.read(&mut buffer).await {
            Ok(0) => embassy_futures::yield_now().await,
            Ok(len) => {
                let data =
                    RxChunk::from_slice(&buffer[..len]).expect("RX chunk capacity too small");
                // 保序交付，队列满时等待；应用必须及时处理，否则 DMA 环形缓冲仍可能溢出。
                uart.rx_queue.send(RxEvent::Data(data)).await;
            }
            Err(error) => {
                uart.record_rx_error(error);
                uart.rx_queue.send(RxEvent::Error(error)).await;
                // 连续错误时也给其他就绪任务执行机会；下次 read 自动恢复接收。
                embassy_futures::yield_now().await;
            }
        }
    }
}
