//! 应用层：启动独立的心跳/TX/RX 任务，main 任务消费接收事件。

use crate::comm::proto::RxState;
use crate::control::pid::Pid;
use crate::runtime::{
    board::{self, Devices},
    config,
    tasks::{self, RxEvent},
};
use embassy_executor::Spawner;
use embassy_stm32::usart::Error;
use embassy_sync::channel::Channel;

pub struct App {
    devices: Devices,
    state: AppState,
}

/// 应用业务状态只由 main 任务修改，无需 Mutex。
pub struct AppState {
    pub pid: Pid,
    pub rx: RxState<{ config::UART_RX_CHUNK_BYTES }>,
    pub last_rx_error: Option<Error>,
}

impl App {
    pub fn init() -> Self {
        Self {
            devices: board::init(),
            state: AppState {
                pid: Pid::new(
                    config::PID_KP,
                    config::PID_KI,
                    config::PID_KD,
                    -config::PID_OUTPUT_MAX,
                    config::PID_OUTPUT_MAX,
                ),
                rx: RxState::new(),
                last_rx_error: None,
            },
        }
    }

    pub async fn run(self, spawner: Spawner) -> ! {
        let Self { devices, mut state } = self;
        let Devices {
            red_led: _red_led,
            green_led,
            uart1,
        } = devices;
        let (tx, rx) = uart1.split();
        // 静态存储由应用组装，任务只拿对应的 Sender / Receiver 能力。
        static TX_QUEUE: tasks::TxQueue = Channel::new();
        static RX_QUEUE: tasks::RxQueue = Channel::new();
        spawner
            .spawn(tasks::uart_tx_task(tx, TX_QUEUE.receiver()))
            .expect("failed to spawn UART TX task");
        spawner
            .spawn(tasks::uart_rx_task(rx, RX_QUEUE.sender()))
            .expect("failed to spawn UART RX task");
        spawner
            .spawn(tasks::heartbeat_task(green_led, TX_QUEUE.sender()))
            .expect("failed to spawn heartbeat task");

        // main 本身就是应用任务。这里只快速处理事件，不等待串口发送或执行忙等。
        // 红灯句柄保留在本任务，panic 仍走独立的 PAC 接管路径。
        loop {
            match RX_QUEUE.receive().await {
                RxEvent::Data(data) => state.rx.on_data(&data),
                RxEvent::Error(error) => {
                    state.last_rx_error = Some(error);
                    state.rx.on_error();
                }
            }
        }
    }
}
