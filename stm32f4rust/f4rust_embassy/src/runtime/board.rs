//! 板级支持：时钟、引脚、DMA 和中断绑定集中在这里。
//! TIM9 由 embassy-time 独占；TIM1/TIM8、TIM2/TIM5 留给电机和编码器。

use crate::drivers::uart::{UartCounters, UartLink};
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
use static_cell::StaticCell;

bind_interrupts!(struct Irqs {
    USART1 => usart::InterruptHandler<peripherals::USART1>;
    DMA2_STREAM7 => dma::InterruptHandler<peripherals::DMA2_CH7>;
    DMA2_STREAM2 => dma::InterruptHandler<peripherals::DMA2_CH2>;
});

pub struct Devices {
    pub red_led: Output<'static>,
    pub green_led: Output<'static>,
    pub uart1: UartLink,
}

pub fn init() -> Devices {
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
    config.rcc.apb1_pre = APBPrescaler::DIV4;
    config.rcc.apb2_pre = APBPrescaler::DIV2;
    // 初始化同时启动 TIM9 时间驱动，不需要另配 SysTick 或 DWT 时基。
    let p = embassy_stm32::init(config);

    // 保持原项目的 GPIO 初始低电平。
    let red_led = Output::new(p.PF9, Level::Low, Speed::Low);
    let green_led = Output::new(p.PF10, Level::Low, Speed::Low);

    let uart_config = usart::Config::default(); // 115200、8N1
    let uart = Uart::new(
        p.USART1,
        p.PA10,
        p.PA9,
        p.DMA2_CH7,
        p.DMA2_CH2,
        Irqs,
        uart_config,
    )
    .expect("invalid USART1 config");

    // DMA 存储必须常驻且位于可由 DMA 访问的 SRAM，不能放进 CCM。
    static RX_BUFFER: StaticCell<[u8; 256]> = StaticCell::new();
    static UART1_COUNTERS: UartCounters = UartCounters::new();
    let uart1 = UartLink::new(uart, RX_BUFFER.init([0; 256]), &UART1_COUNTERS);
    Devices {
        red_led,
        green_led,
        uart1,
    }
}
