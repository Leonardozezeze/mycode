//! 周期上报实测转速和目标转速。

use super::speed_control;
use crate::{
    comm::telemetry::{CAN_SPEED_FRAME_ID, encode_speed_payload},
    drivers::can::CanLink,
    runtime::config::can::CAN_SPEED_TELEMETRY_PERIOD_MS,
};
use embassy_stm32::can::Frame;
use embassy_sync::channel::TrySendError;
use embassy_time::{Duration, Instant, Timer};

/// 将一帧转速遥测非阻塞地放入 CAN 发送队列。
pub(in crate::runtime) fn try_send_speed(
    link: &CanLink,
    measured_rpm: f32,
    target_rpm: f32,
) -> Result<(), TrySendError<Frame>> {
    let payload = encode_speed_payload(measured_rpm, target_rpm);
    let frame = Frame::new_standard(CAN_SPEED_FRAME_ID, &payload)
        .expect("invalid CAN speed telemetry frame");
    link.try_send(frame)
}

#[embassy_executor::task]
pub async fn can_telemetry_task(link: &'static CanLink) {
    let period = Duration::from_millis(CAN_SPEED_TELEMETRY_PERIOD_MS);
    let mut deadline = Instant::now() + period;

    loop {
        Timer::at(deadline).await;
        // 从本次唤醒重新计时，错过的周期不补发。
        deadline = Instant::now() + period;
        let speed = speed_control::status();
        // 队列满时跳过本帧，CanLink 会记录队列满次数。
        let _ = try_send_speed(link, speed.rpm, speed.target_rpm);
    }
}
