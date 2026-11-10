//! 板级支持（BSP）：时钟配置、引脚映射、板载外设实例化。
//! 与板子相关（换芯片/晶振/引脚）的代码只改这个文件。
//!
//! 产出按准入测试分两路上交：
//! **直接交给 App 使用的 → [`Board`]；需先经 driver 包装的 → [`DeviceParts`]**。
//! 配置参数（波特率、引脚 AF 等）属于"板子事实"，全部在本文件定死。

use stm32f4xx_hal::{
    gpio::{self, Output, PushPull},
    pac::{self, USART1},
    prelude::*,
    rcc::{Clocks, Config},
    serial::{Rx, Tx},
};

/// App 直接使用的板级琐碎资源
pub struct Board {
    /// 板载 LED（PF9 / PF10）
    pub red_led: gpio::PF9<Output<PushPull>>,
    pub green_led: gpio::PF10<Output<PushPull>>,
    /// 时钟状态，初始化外设时使用
    #[allow(dead_code)] // 骨架期暂未使用
    pub clocks: Clocks,
}

/// 交给 drivers 包装的器件零件（配置完毕的半成品）
pub struct DeviceParts {
    /// USART1：PA9=TX，PA10=RX，115200-8N1
    pub uart1_tx: Tx<USART1>,
    pub uart1_rx: Rx<USART1>,
}

/// 板级初始化：时钟树 → DWT 时基 → GPIO → USART1。
///
/// PAC 外设句柄在这里一次性 `take()`；DWT / DCB 是一次性使能的
/// 内核设施，直接交给 `drivers::dwt` 消耗掉。
pub fn init() -> (Board, DeviceParts) {
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

    (
        Board {
            red_led,
            green_led,
            clocks: rcc.clocks,
        },
        DeviceParts { uart1_tx, uart1_rx },
    )
}
