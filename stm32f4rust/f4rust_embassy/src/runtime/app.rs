//! 应用层：接收初始化好的设备并启动心跳、调试串口接收任务。

use crate::runtime::{
    board::Devices,
    tasks::{debug_uart, heartbeat},
};
use embassy_executor::Spawner;

pub async fn run(devices: Devices, spawner: Spawner) -> ! {
    spawner
        .spawn(debug_uart::debug_uart_task(devices.uart1))
        .expect("failed to spawn debug UART task");
    spawner
        .spawn(heartbeat::heartbeat_task(devices.green_led, devices.uart1))
        .expect("failed to spawn heartbeat task");
    // 主任务挂起，保留红灯句柄；panic 仍走独立的 PAC 接管路径。
    core::future::pending().await
}
