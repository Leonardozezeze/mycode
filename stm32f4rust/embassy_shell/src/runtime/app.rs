//! 应用层：启动调试串口、心跳和 CAN 任务。
use crate::{
    drivers::can::CanLink,
    runtime::{
        board::Devices,
        shell::commands::{bind_can1, bind_uart1},
        tasks::{can_shell_task, can_task, heartbeat, uart_shell_task},
    },
};
use embassy_executor::Spawner;
use embassy_stm32::can::Can;

static CAN1_LINK: CanLink = CanLink::new();

pub async fn run(devices: Devices, spawner: Spawner) -> ! {
    bind_uart1(devices.uart1);
    spawner
        .spawn(uart_shell_task::uart_shell_task(devices.uart1))
        .expect("failed to spawn UART shell task");

    spawner
        .spawn(heartbeat::heartbeat_task(devices.green_led))
        .expect("failed to spawn heartbeat task");
    spawner
        .spawn(can_start_task(devices.can, spawner))
        .expect("failed to spawn CAN startup task");

    // 主任务挂起，保留红灯句柄；panic 仍走独立的 PAC 接管路径。
    core::future::pending().await
}

#[embassy_executor::task]
async fn can_start_task(mut can: Can<'static>, spawner: Spawner) {
    let can_link = &CAN1_LINK;
    can.enable().await;
    let (tx, rx) = can.split();
    spawner
        .spawn(can_task::can_tx_task(tx, can_link))
        .expect("can_tx_task init failed");
    spawner
        .spawn(can_task::can_rx_task(rx, can_link))
        .expect("can_rx_task init failed");
    spawner
        .spawn(can_shell_task::can_shell_task(can_link))
        .expect("can_shell_task init failed");
    bind_can1(can_link);

    // 保持 CAN 句柄存活，避免 Drop 重置正在收发的外设。
    core::future::pending().await
}
