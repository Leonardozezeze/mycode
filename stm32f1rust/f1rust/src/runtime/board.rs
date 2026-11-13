//! 板级支持（BSP）：时钟配置、引脚映射、外设初始化和驱动组装。
//! 与板子相关（换芯片/晶振/引脚）的代码只改这个文件。
//!
//! 返回已经初始化和组装好的设备 [`Devices`]，App 直接使用。
//! 时钟、引脚映射和波特率在本文件配置，电机参数由 runtime::config 提供。

use crate::drivers::uart::{self, UartLink};
use stm32f1xx_hal::{
    gpio::{self, IOPinSpeed, Output, OutputSpeed, PinState, PushPull},
    pac::{self, USART1},
    prelude::*,
    rcc::Config,
};

/// 初始化并组装完毕的设备，交给 App 持有和调用。
pub struct Devices {
    /// 板载 LED（PC14 / PC13）
    pub red_led: gpio::PC14<Output<PushPull>>,
    pub green_led: gpio::PC13<Output<PushPull>>,
    pub uart1: UartLink<USART1>,
}

/// 板级初始化：时钟树 → DWT 时基 → GPIO/USART/PWM → 驱动组装。
///
/// PAC 外设句柄在这里一次性 `take()`；DWT / DCB 是一次性使能的
/// 内核设施，直接交给 `drivers::dwt` 消耗掉。
/// UART RX 在组装时交给串口驱动的中断侧，由驱动开启接收中断。
pub fn init() -> Devices {
    let dp = pac::Peripherals::take().unwrap();
    let cp = cortex_m::peripheral::Peripherals::take().unwrap();

    // 外部 8MHz 晶振，F103 系统时钟 72MHz，APB1 为 36MHz。
    let mut flash = dp.FLASH.constrain();
    let mut rcc = dp.RCC.freeze(
        Config::hse(8.MHz()).sysclk(72.MHz()).pclk1(36.MHz()),
        &mut flash.acr,
    );

    // 系统时基：每微秒周期数由 HCLK 推得，不在 dwt 里写死
    crate::drivers::dwt::DWT::init(cp.DCB, cp.DWT, rcc.clocks.hclk().raw());

    let mut gpioc = dp.GPIOC.split(&mut rcc);
    // 按低电平点亮接线，初始置高保持熄灭。
    let mut red_led = gpioc
        .pc14
        .into_push_pull_output_with_state(&mut gpioc.crh, PinState::High);
    let mut green_led = gpioc
        .pc13
        .into_push_pull_output_with_state(&mut gpioc.crh, PinState::High);

    red_led.set_speed(&mut gpioc.crh, IOPinSpeed::Mhz2);
    green_led.set_speed(&mut gpioc.crh, IOPinSpeed::Mhz2);

    let mut gpioa = dp.GPIOA.split(&mut rcc);
    // USART1 默认映射：PA9 TX（复用推挽），PA10 RX（浮空输入）。
    let (uart1_tx, uart1_rx) = dp
        .USART1
        .serial(
            (
                gpioa.pa9.into_alternate_push_pull(&mut gpioa.crh),
                gpioa.pa10,
            ),
            stm32f1xx_hal::serial::Config::default().baudrate(115_200.bps()),
            &mut rcc,
        )
        .split();

    let uart1 = uart::uart1(uart1_tx, uart1_rx);
    Devices {
        red_led,
        green_led,
        uart1,
    }
}
