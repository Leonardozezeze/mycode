//! 应用层：初始化设备并启动调试串口、心跳、遥测和含杂率联动推杆任务。
use crate::{
    comm::virsenser,
    drivers::{adc_seed, can::CanLink},
    runtime::{
        board::Devices,
        shell::commands::{bind_can1, bind_uart1},
        tasks::{can_shell_task, can_task, heartbeat, uart_shell_task},
    },
};
use embassy_executor::Spawner;

static CAN1_LINK: CanLink = CanLink::new();

pub async fn run(mut devices: Devices, spawner: Spawner) -> ! {
    // 在任何虚拟传感器更新之前完成种子采集和全局 RNG 初始化。
    let seed = adc_seed::get_adc_seed64(devices.adc1, devices.adc_noise_pin).await;
    virsenser::rng_init(seed);
    let can_link = &CAN1_LINK;
    bind_can1(can_link);
    bind_uart1(devices.uart1);
    spawner
        .spawn(uart_shell_task::uart_shell_task(devices.uart1))
        .expect("failed to spawn UART shell task");

    devices.can.enable().await;
    let (tx, rx) = devices.can.split();
    spawner
        .spawn(heartbeat::heartbeat_task(devices.green_led))
        .expect("failed to spawn heartbeat task");
    spawner
        .spawn(can_task::can_tx_task(tx, can_link))
        .expect("can_tx_task init failed");
    spawner
        .spawn(can_task::can_rx_task(rx, can_link))
        .expect("can_rx_task init failed");
    spawner
        .spawn(can_shell_task::can_shell_task(can_link))
        .expect("can_shell_task init failed");
    // 主任务挂起，保留红灯句柄；panic 仍走独立的 PAC 接管路径。
    core::future::pending().await
}
