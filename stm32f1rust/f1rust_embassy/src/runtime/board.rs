//! 板级支持：时钟、引脚、DMA 和中断绑定集中在这里。
//! TIM2 由 embassy-time 独占；TIM1/TIM3/TIM4 留给电机和编码器。

use crate::drivers::uart::{UartCounters, UartLink};
use embassy_stm32::{
    bind_interrupts, dma,
    gpio::{Level, Output, Speed},
    peripherals,
    rcc::{AHBPrescaler, APBPrescaler, Hse, HseMode, Pll, PllMul, PllPreDiv, PllSource, Sysclk},
    time::Hertz,
    usart::{self, Uart},
};
use static_cell::StaticCell;

bind_interrupts!(struct Irqs {
    USART1 => usart::InterruptHandler<peripherals::USART1>;
    DMA1_CHANNEL4 => dma::InterruptHandler<peripherals::DMA1_CH4>;
    DMA1_CHANNEL5 => dma::InterruptHandler<peripherals::DMA1_CH5>;
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
    // 8 MHz * 9 = 72 MHz；APB1 分频后为 F103 允许的 36 MHz。
    config.rcc.pll = Some(Pll {
        src: PllSource::HSE,
        prediv: PllPreDiv::DIV1,
        mul: PllMul::MUL9,
    });
    config.rcc.sys = Sysclk::PLL1_P;
    config.rcc.ahb_pre = AHBPrescaler::DIV1;
    config.rcc.apb1_pre = APBPrescaler::DIV2;
    config.rcc.apb2_pre = APBPrescaler::DIV1;
    // 初始化同时启动 TIM2 时间驱动，不需要另配 SysTick 或 DWT 时基。
    let p = embassy_stm32::init(config);

    // PC13/PC14 的 LED 为低电平点亮，初始化为熄灭。
    let red_led = Output::new(p.PC14, Level::High, Speed::Low);
    let green_led = Output::new(p.PC13, Level::High, Speed::Low);

    let uart_config = usart::Config::default(); // 115200、8N1
    let uart = Uart::new(
        p.USART1,
        p.PA10,
        p.PA9,
        p.DMA1_CH4,
        p.DMA1_CH5,
        Irqs,
        uart_config,
    )
    .expect("invalid USART1 config");

    // DMA 缓冲必须常驻于 SRAM。
    static RX_BUFFER: StaticCell<[u8; 256]> = StaticCell::new();
    static UART1_COUNTERS: UartCounters = UartCounters::new();
    let uart1 = UartLink::new(uart, RX_BUFFER.init([0; 256]), &UART1_COUNTERS);
    Devices {
        red_led,
        green_led,
        uart1,
    }
}
