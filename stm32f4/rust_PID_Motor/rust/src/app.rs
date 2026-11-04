use crate::{algo, peripheral::{self, dwt::DWT, uart::{Uart, UartKind}}, u1println};

unsafe extern "C"{
    unsafe fn board_drive_init();
}
/// 应用初始化
pub fn app_init() {
    DWT::init();
    unsafe {
        board_drive_init();
    }
    u1println!("APP Init Okay!!!");
}

/// 破碎率 b 的映射范围
const B_MIN: f32 = 3.0;
const B_MAX: f32 = 4.0;
/// 目标转速范围（RPM）
const RPM_MIN: f32 = 50.0;
const RPM_MAX: f32 = 250.0;

/// 破碎率 b → 目标转速，线性反比；越界按边界算
fn breakage_rate_to_target(b: f32) -> f32 {
    let b = b.clamp(B_MIN, B_MAX);
    RPM_MAX - (b - B_MIN) * (RPM_MAX - RPM_MIN) / (B_MAX - B_MIN)
}

/// 应用主循环
pub fn app_run() -> ! {
    let mut green = peripheral::led::Led::new(peripheral::led::LedColor::Green);
    let mut enc=peripheral::encoder::EncoderSpeed::new(crate::config::COUNTS_PER_REV);
    let mut pid=algo::pid::Pid::new(0.3, 0.2, 0.0,0.0, -1000.0, 1000.0);
    let motor=peripheral::motor::Motor::new();
    let uart4 = Uart::new(UartKind::Uart4);
    let mut parser = algo::frame::FrameParser::new();
    motor.set_speed(500);
    loop {
        let now_ms=DWT::uptime_ms();
        green.toggle_task(1000, now_ms);
        // 收 UART4 数据，凑满一帧就处理
        while let Some(byte) = uart4.read_byte() {
            if let Some(frame) = parser.feed(byte) {
                let target = breakage_rate_to_target(frame.b);
                pid.set_target(target);
                u1println!("b={} i={} -> target={}", frame.b, frame.i, target);
            }
        }
        if let Some(rpm)=enc.update_task(now_ms, 100)
        {
            u1println!("now_rpm:{}",rpm);
            let pwm_output=pid.update(rpm);
            motor.set_speed(pwm_output as i16);
        }
    }
}
