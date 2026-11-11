//! 板级支持（BSP）：时钟配置、引脚映射、外设初始化和驱动组装。
//! 与板子相关（换芯片/晶振/引脚）的代码只改这个文件。
//!
//! 返回已经初始化和组装好的设备 [`Devices`]，App 直接使用。
//! 时钟、引脚映射和波特率在本文件配置，电机参数由 runtime::config 提供。

use crate::drivers::{
    enc::Encoder,
    motor::Motor,
    uart::{self, UartLink},
};
use crate::runtime::config;
use stm32f4xx_hal::{
    gpio::{self, Output, PushPull},
    pac::{self, TIM3, TIM4, UART5, USART1},
    prelude::*,
    rcc::Config,
    timer::PwmExt,
};

/// 初始化并组装完毕的设备，交给 App 持有和调用。
pub struct Devices {
    /// 板载 LED（PF9 / PF10）
    pub red_led: gpio::PF9<Output<PushPull>>,
    pub green_led: gpio::PF10<Output<PushPull>>,
    pub uart1: UartLink<USART1>,
    /// 传感器串口：PC12 TX / PD2 RX，115200、8N1。
    pub uart5: UartLink<UART5>,
    /// 本板电机：TIM3 CH1、PA4/PA5 方向引脚。
    pub motor: Motor<TIM3, 0, 'A', 4, 'A', 5>,
    /// TIM4 编码器：PD12/PD13 接 AB 相，AF2。
    pub encoder: Encoder<TIM4>,
}

/// 板级初始化：时钟树 → DWT 时基 → GPIO/USART/PWM → 驱动组装。
///
/// PAC 外设句柄在这里一次性 `take()`；DWT / DCB 是一次性使能的
/// 内核设施，直接交给 `drivers::dwt` 消耗掉。
/// UART RX 在组装时交给串口驱动的中断侧，由驱动开启接收中断。
pub fn init() -> Devices {
    let dp = pac::Peripherals::take().unwrap();
    let cp = cortex_m::peripheral::Peripherals::take().unwrap();

    // 外部 8MHz 晶振，系统时钟 168MHz
    let mut rcc = dp.RCC.freeze(Config::hse(8.MHz()).sysclk(168.MHz()));

    // 系统时基：每微秒周期数由 HCLK 推得，不在 dwt 里写死
    crate::drivers::dwt::DWT::init(cp.DCB, cp.DWT, rcc.clocks.hclk().to_Hz());

    let gpiof = dp.GPIOF.split(&mut rcc);
    let red_led = gpiof.pf9.into_push_pull_output();
    let green_led = gpiof.pf10.into_push_pull_output();

    let gpioa = dp.GPIOA.split(&mut rcc);
    let (uart1_tx, uart1_rx) = dp
        .USART1
        .serial(
            (
                gpioa.pa9.into_alternate::<7>(),
                gpioa.pa10.into_alternate::<7>(),
            ),
            115_200.bps(),
            &mut rcc,
        )
        .unwrap() // Err 只在字长/校验配置非法时出现，8N1 不会
        .split();

    let gpioc = dp.GPIOC.split(&mut rcc);
    let gpiod = dp.GPIOD.split(&mut rcc);
    let (uart5_tx, uart5_rx) = dp
        .UART5
        .serial(
            (
                gpioc.pc12.into_alternate::<8>(),
                gpiod.pd2.into_alternate::<8>(),
            ),
            115_200.bps(), // 默认 8 数据位、无校验、1 停止位
            &mut rcc,
        )
        .unwrap()
        .split();

    let (pwm_mgr, (pwm_ch1, ..)) = dp.TIM3.pwm_hz(config::MOTOR_PWM_HZ.Hz(), &mut rcc);
    let pwm_ch1 = pwm_ch1.with(gpioa.pa6.into_alternate::<2>());
    let ain1 = gpioa.pa4.into_push_pull_output();
    let ain2 = gpioa.pa5.into_push_pull_output();

    let encoder_a = gpiod.pd12.into_alternate::<2>(); // A 相：TIM4_CH1
    let encoder_b = gpiod.pd13.into_alternate::<2>(); // B 相：TIM4_CH2
    let mut encoder = dp.TIM4.qei((encoder_a, encoder_b), &mut rcc);
    encoder.set_count(0);

    let uart1 = uart::uart1(uart1_tx, uart1_rx);
    let uart5 = uart::uart5(uart5_tx, uart5_rx);
    let motor = Motor::new(pwm_mgr, pwm_ch1, ain1, ain2);
    let encoder = Encoder::new(encoder);
    Devices {
        red_led,
        green_led,
        uart1,
        uart5,
        motor,
        encoder,
    }
}
