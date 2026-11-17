use crate::runtime::board::{self, Devices};
use crate::runtime::tasks;
use embassy_sync::channel::Channel;
pub struct App {
    devices: Devices,
}
impl App {
    pub fn init() -> Self {
        Self {
            devices: board::init(),
        }
    }
    pub async fn run(self, spawner: embassy_executor::Spawner) -> ! {
        let Devices {
            red_led,
            green_led,
            tx,
            rx,
        } = self.devices;
        static TX_QUEUE: tasks::TxQueue = Channel::new();
        let tx_sender = TX_QUEUE.sender();
        let tx_receiver = TX_QUEUE.receiver();
        spawner
            .spawn(tasks::uart_tx_task(tx, tx_receiver))
            .expect("failed to spawn UART TX task");
        let message = tasks::TxMessage::from_slice(b"UART ready\r\n").expect("message too long");
        tx_sender.send(message).await;
        static RX_QUEUE: tasks::RxQueue = Channel::new();
        spawner
            .spawn(tasks::uart_rx_task(rx, RX_QUEUE.sender()))
            .expect("failed to spawn UART RX task");
        loop {
            match RX_QUEUE.receive().await {
                tasks::RxEvent::Data(data) => {
                    // 下一步交给通信层处理
                    let message =
                        tasks::TxMessage::from_slice(b"fafhakfaj\r\n").expect("TX message capacity too small");
                    tx_sender.send(message).await;
                }
                tasks::RxEvent::Error(error) => {
                    // 下一步记录错误，并通知通信层
                }
            }
        }
    }
}
