//! 板级支持（BSP）：时钟配置、引脚映射、外设初始化和驱动组装。
//! 与板子相关（换芯片/晶振/引脚）的代码只改这个文件。
//!
//! 返回已经初始化和组装好的设备 [`Devices`]，App 直接使用。
//! 时钟、引脚映射和波特率在本文件配置，电机参数由 runtime::config 提供。

use crate::drivers::{
    uart::{self, UartLink},
};
use stm32f4xx_hal::{
    gpio::{self, Output, PushPull},
    pac::{self, USART1},
    prelude::*,
    rcc::Config,
};

/// 初始化并组装完毕的设备，交给 App 持有和调用。
pub struct Devices {
    /// 板载 LED（PF9 / PF10）
    pub red_led: gpio::PF9<Output<PushPull>>,
    pub green_led: gpio::PF10<Output<PushPull>>,
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

    let uart1 = uart::uart1(uart1_tx, uart1_rx);
    Devices {
        red_led,
        green_led,
        uart1,
    }
}
