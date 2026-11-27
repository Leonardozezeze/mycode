//! 周期更新虚拟品质数据，并通过 CAN 上报。

use crate::{
    comm::{
        telemetry::{CAN_RATE_FRAME_ID, encode_rate_payload},
        virsenser::VirtualSensor,
    },
    drivers::can::CanLink,
    runtime::config::{
        MAX_BROKEN_RATE, MAX_IMPURITY_RATE, MIN_BROKEN_RATE, MIN_IMPURITY_RATE,
        VIRTUAL_SENSOR_PERIOD_MS,
    },
};
use embassy_stm32::can::Frame;
use embassy_time::{Duration, Ticker};

#[embassy_executor::task]
pub async fn can_telemetry_task(link: &'static CanLink) {
    let step = 0.007;
    let spike_prob = 30;
    let mut broken = VirtualSensor::new(
        MIN_BROKEN_RATE,
        MAX_BROKEN_RATE,
        (MIN_BROKEN_RATE + MAX_BROKEN_RATE) / 2.0,
        step,
        spike_prob,
    );
    let mut impurity = VirtualSensor::new(
        MIN_IMPURITY_RATE,
        MAX_IMPURITY_RATE,
        (MIN_IMPURITY_RATE + MAX_IMPURITY_RATE) / 2.0,
        step,
        spike_prob,
    );
    let mut ticker = Ticker::every(Duration::from_millis(VIRTUAL_SENSOR_PERIOD_MS));

    loop {
        ticker.next().await;
        broken.update();
        impurity.update();

        let payload = encode_rate_payload(impurity.value, broken.value);
        let frame = Frame::new_standard(CAN_RATE_FRAME_ID, &payload)
            .expect("invalid CAN telemetry frame configuration");
        // 队列满时跳过本次上报，维持采样周期；try_send 会记录队列满次数。
        let _ = link.try_send(frame);
    }
}
