//! 板级支持：时钟、引脚、DMA 和中断绑定集中在这里。
//! TIM2 由 embassy-time 独占；TIM1/TIM3/TIM4 留给电机和编码器。

use super::config::{CAN1_BITRATE, UART1_BAUD_RATE, UART1_RX_DMA_BUFFER_BYTES};
use crate::drivers::uart::{UartLink, init_uart};
use embassy_executor::Spawner;
use embassy_stm32::{
    bind_interrupts,
    can::{self, Can, Fifo, filter::Mask32},
    dma,
    gpio::{Level, Output, Speed},
    peripherals,
    rcc::{AHBPrescaler, APBPrescaler, Hse, HseMode, Pll, PllMul, PllPreDiv, PllSource, Sysclk},
    time::Hertz,
    usart::{self, Uart},
};

bind_interrupts!(struct Irqs {
    USART1 => usart::InterruptHandler<peripherals::USART1>;
    DMA1_CHANNEL4 => dma::InterruptHandler<peripherals::DMA1_CH4>;
    DMA1_CHANNEL5 => dma::InterruptHandler<peripherals::DMA1_CH5>;
    USB_HP_CAN1_TX  => can::TxInterruptHandler<peripherals::CAN>;
    USB_LP_CAN1_RX0 => can::Rx0InterruptHandler<peripherals::CAN>;
    CAN1_RX1        => can::Rx1InterruptHandler<peripherals::CAN>;
    CAN1_SCE        => can::SceInterruptHandler<peripherals::CAN>;
});

pub struct Devices {
    pub red_led: Output<'static>,
    pub green_led: Output<'static>,
    /// USART1 用于调试日志输出和调试数据接收。
    pub uart1: &'static UartLink,
    pub can: Can<'static>,
}

pub fn init(spawner: Spawner) -> Devices {
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
    config.rcc.apb1_pre = APBPrescaler::DIV2; // 36 MHz
    config.rcc.apb2_pre = APBPrescaler::DIV1; // 72 MHz
    // 初始化同时启动 TIM2 时间驱动，不需要另配 SysTick 或 DWT 时基。
    let p = embassy_stm32::init(config);

    // PC14/PC13 的 LED 为低电平点亮，初始化为熄灭。
    let red_led = Output::new(p.PC14, Level::High, Speed::Low);
    let green_led = Output::new(p.PC13, Level::High, Speed::Low);

    let mut uart_config = usart::Config::default(); // 8N1
    uart_config.baudrate = UART1_BAUD_RATE;
    let uart1 = Uart::new(
        p.USART1,
        p.PA10,
        p.PA9,
        p.DMA1_CH4,
        p.DMA1_CH5,
        Irqs,
        uart_config,
    )
    .expect("invalid USART1 config");
    // 创建独立的静态 DMA 缓冲和串口实例，并启动后台收发任务。
    let uart1 = init_uart!(spawner, uart1, UART1_RX_DMA_BUFFER_BYTES);

    // CAN 重映射到 PB8/PB9，腾出 PA11/PA12。
    let mut can = Can::new(p.CAN, p.PB8, p.PB9, Irqs); // 参数顺序是 RX、TX
    can.set_bitrate(CAN1_BITRATE);
    can.modify_filters()
        .enable_bank(0, Fifo::Fifo0, Mask32::accept_all());
    Devices {
        red_led,
        green_led,
        uart1,
        can,
    }
}
