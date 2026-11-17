//! 应用层：初始化设备并启动心跳、调试串口接收任务。

use crate::runtime::{
    board::{self, Devices},
    tasks::{debug_uart, heartbeat},
};
use embassy_executor::Spawner;

pub struct App {
    devices: Devices,
}

impl App {
    pub fn init(spawner: Spawner) -> Self {
        Self {
            devices: board::init(spawner),
        }
    }

    pub async fn run(self, spawner: Spawner) -> ! {
        let Self { devices } = self;
        let Devices {
            red_led: _red_led,
            green_led,
            uart1,
        } = devices;
        spawner
            .spawn(debug_uart::debug_uart_task(uart1))
            .expect("failed to spawn debug UART task");
        spawner
            .spawn(heartbeat::heartbeat_task(green_led, uart1))
            .expect("failed to spawn heartbeat task");
        // 主任务挂起，保留红灯句柄；panic 仍走独立的 PAC 接管路径。
        core::future::pending().await
    }
}
