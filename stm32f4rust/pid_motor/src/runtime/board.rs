//! 板级支持：时钟、引脚、DMA 和中断绑定集中在这里。
//! TIM9 由 embassy-time 独占；TIM1/TIM8、TIM2/TIM5 留给电机和编码器。
//! TIM9 共用 TIM1_BRK_TIM9 中断，后续使用 TIM1 刹车功能时需协调中断处理。

use crate::drivers::uart::{UartLink, init_uart};
use embassy_executor::Spawner;
use embassy_stm32::{
    bind_interrupts, dma,
    gpio::{Level, Output, Speed},
    peripherals,
    rcc::{
        AHBPrescaler, APBPrescaler, Hse, HseMode, Pll, PllMul, PllPDiv, PllPreDiv, PllQDiv,
        PllSource, Sysclk,
    },
    time::Hertz,
    usart::{self, Uart},
};

bind_interrupts!(struct Irqs {
    USART1 => usart::InterruptHandler<peripherals::USART1>;
    DMA2_STREAM7 => dma::InterruptHandler<peripherals::DMA2_CH7>;
    DMA2_STREAM2 => dma::InterruptHandler<peripherals::DMA2_CH2>;
});

pub struct Devices {
    pub red_led: Output<'static>,
    pub green_led: Output<'static>,
    /// USART1 用于调试日志输出和调试数据接收。
    pub uart1: &'static UartLink,
}

pub fn init(spawner: Spawner) -> Devices {
    let mut config = embassy_stm32::Config::default();
    config.rcc.hse = Some(Hse {
        freq: Hertz(8_000_000),
        mode: HseMode::Oscillator,
    });
    // 8 MHz / 8 * 336 / 2 = 168 MHz；PLLQ 同时提供 48 MHz。
    config.rcc.pll_src = PllSource::HSE;
    config.rcc.pll = Some(Pll {
        prediv: PllPreDiv::DIV8,
        mul: PllMul::MUL336,
        divp: Some(PllPDiv::DIV2),
        divq: Some(PllQDiv::DIV7),
        divr: None,
    });
    config.rcc.sys = Sysclk::PLL1_P;
    config.rcc.ahb_pre = AHBPrescaler::DIV1;
    config.rcc.apb1_pre = APBPrescaler::DIV4; // 42 MHz
    config.rcc.apb2_pre = APBPrescaler::DIV2; // 84 MHz
    // 初始化同时启动 TIM9 时间驱动，不需要另配 SysTick 或 DWT 时基。
    let p = embassy_stm32::init(config);

    // PF9/PF10 的 LED 为低电平点亮，初始化为熄灭。
    let red_led = Output::new(p.PF9, Level::High, Speed::Low);
    let green_led = Output::new(p.PF10, Level::High, Speed::Low);

    let uart_config = usart::Config::default(); // 115200、8N1
    let uart1 = Uart::new(
        p.USART1,
        p.PA10,
        p.PA9,
        p.DMA2_CH7,
        p.DMA2_CH2,
        Irqs,
        uart_config,
    )
    .expect("invalid USART1 config");

    // 创建独立的静态 DMA 缓冲和串口实例，并启动后台收发任务。
    // F407 的 DMA 无法访问 CCM；memory.x 将缓冲和任务存储放在普通 SRAM。
    let uart1 = init_uart!(spawner, uart1, 256);
    Devices {
        red_led,
        green_led,
        uart1,
    }
}
