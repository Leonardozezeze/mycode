//! 心跳任务：定时闪灯，并以非阻塞方式发送心跳日志。
use crate::drivers::uart::{UartLink, uart_print};
use crate::runtime::config;
use core::sync::atomic::{AtomicU32, Ordering};
use embassy_stm32::gpio::Output;
use embassy_time::{Duration, Instant, Ticker};

/// 心跳日志入队失败的累计次数，按 u32 回绕；不包含 UART 硬件错误。
static HEARTBEAT_LOG_DROPPED: AtomicU32 = AtomicU32::new(0);

pub fn heartbeat_log_dropped() -> u32 {
    HEARTBEAT_LOG_DROPPED.load(Ordering::Relaxed)
}

#[embassy_executor::task]
pub async fn heartbeat_task(mut led: Output<'static>, uart1: &'static UartLink) {
    let mut ticker = Ticker::every(Duration::from_millis(config::HEARTBEAT_PERIOD_MS));
    loop {
        ticker.next().await;
        led.toggle();
        // 日志拥塞不能延迟下一次闪灯；整条拒绝，不会只入队半条消息。
        if uart_print!(uart1, "tick {}ms\r\n", Instant::now().as_millis()).is_err() {
            HEARTBEAT_LOG_DROPPED.fetch_add(1, Ordering::Relaxed);
        }
    }
}
