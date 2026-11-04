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
            // 每个 f32 占 4 字节，两个拼成 8 字节
            let mut buf = [0u8; 11];
            buf[0]=0xFF;
            buf[9]=0;
            buf[10]=0xEE;
            buf[1..5].copy_from_slice(&b.to_le_bytes());
            buf[5..9].copy_from_slice(&i.to_le_bytes());
            // 如果 uart2_send_data 签名是 (*const u8, usize)：
            uart2_send_data(buf.as_ptr(), 11);
        }
        led.toggle_task(now);
    }
}
