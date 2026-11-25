use crate::drivers::can::CanLink;
use embassy_stm32::can::{CanRx, CanTx};
#[embassy_executor::task]
pub async fn can_tx_task(mut tx: CanTx<'static>, link: &'static CanLink) {
    loop {
        let frame = link.tx_queue.receive().await;
        tx.write(&frame).await;
    }
}
#[embassy_executor::task]
pub async fn can_rx_task(mut rx: CanRx<'static>, link: &'static CanLink) {
    loop {
        let event = rx.read().await;
        if let Err(error) = &event {
            link.record_bus_error(error);
        }
        link.rx_queue.send(event).await;
    }
}
