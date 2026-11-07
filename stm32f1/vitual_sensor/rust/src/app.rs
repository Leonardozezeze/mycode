use crate::{
    algo::virsenser::{SensorTask, rng_init},
    peripheral::{self, delay::now_ms, uart2::uart2_send_data},
    u1println,
};
unsafe extern "C" {
    unsafe fn board_drive_init();
}
/// 应用初始化
pub fn app_init() {
    unsafe {
        board_drive_init();
        rng_init();
    }
    u1println!("APP Init Okay!!!");
}
/// 应用主循环
pub fn app_run() -> ! {
    let led = peripheral::led::Led::new();
    let mut sensor = SensorTask::new();
    loop {
        let now = now_ms();
        if let Some((b, i)) = sensor.poll(now) {
            u1println!("breakage:{},impurity:{}", b, i);

            // TODO: 电机转速 / 目标转速尚未接入，先固定发 1.0
            const RPM: f32 = 1.0;
            const TARGET_RPM: f32 = 1.0;

            // 帧格式（18 字节，小端）：
            // 0xFF | 破碎率 f32 | 含杂率 f32 | 电机转速 f32 | 目标转速 f32 | 0xEE
            let mut buf = [0u8; 18];
            buf[0] = 0xFF;
            buf[1..5].copy_from_slice(&b.to_le_bytes());
            buf[5..9].copy_from_slice(&i.to_le_bytes());
            buf[9..13].copy_from_slice(&RPM.to_le_bytes());
            buf[13..17].copy_from_slice(&TARGET_RPM.to_le_bytes());
            buf[17] = 0xEE;
            uart2_send_data(buf.as_ptr(), buf.len() as u16);
        }
        led.toggle_task(now);
    }
}
