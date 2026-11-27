//! 心跳任务：定时闪灯，发送串口日志和 CAN 演示帧。
use crate::runtime::config::heartbeat::HEARTBEAT_PERIOD_MS;
use embassy_stm32::gpio::Output;
use embassy_time::{Duration, Ticker};

#[embassy_executor::task]
pub async fn heartbeat_task(mut led: Output<'static>) {
    let mut ticker = Ticker::every(Duration::from_millis(HEARTBEAT_PERIOD_MS));
    loop {
        ticker.next().await;
        led.toggle();
    }
}
