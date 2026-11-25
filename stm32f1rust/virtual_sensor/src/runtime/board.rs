//! 板级支持：时钟、引脚、DMA 和中断绑定集中在这里。
//! TIM2 由 embassy-time 独占；TIM1/TIM3/TIM4 留给电机和编码器。

use crate::drivers::{
    liner_actuator::LinearActuator,
    uart::{UartLink, init_uart},
};
use embassy_executor::Spawner;
use embassy_stm32::{
    Peri, adc::{self, Adc}, bind_interrupts, can::{self, Can, Fifo, filter::Mask32}, dma, gpio::{Level, Output, OutputOpenDrain, Speed, SwjCfg}, peripherals::{self}, rcc::{
        ADCPrescaler, AHBPrescaler, APBPrescaler, Hse, HseMode, Pll, PllMul, PllPreDiv, PllSource,
        Sysclk,
    }, time::Hertz, usart::{self, Uart},
};

bind_interrupts!(struct Irqs {
    USART1 => usart::InterruptHandler<peripherals::USART1>;
    DMA1_CHANNEL4 => dma::InterruptHandler<peripherals::DMA1_CH4>;
    DMA1_CHANNEL5 => dma::InterruptHandler<peripherals::DMA1_CH5>;
    ADC1_2 => adc::InterruptHandler<peripherals::ADC1>;
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
    /// ADC1 和悬空 PA0 仅在启动时用于采集种子噪声。
    pub adc1: Adc<'static, peripherals::ADC1>,
    pub adc_noise_pin: Peri<'static, peripherals::PA0>,
    /// K1 接 PB4（伸出），K2 接 PB5（缩回），低电平有效。
    pub linear_actuator: LinearActuator,
    pub can:Can<'static>,
}

pub fn init(spawner: Spawner) -> Devices {
    let mut config = embassy_stm32::Config::default();
    // PB4 默认用于 JTAG，释放 JTAG 引脚，保留 PA13/PA14 的 SWD 调试。
    config.swj = SwjCfg::SwdOnly;
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
    config.rcc.adc_pre = ADCPrescaler::DIV6;
    // 初始化同时启动 TIM2 时间驱动，不需要另配 SysTick 或 DWT 时基。
    let p = embassy_stm32::init(config);

    // PC13/PC14 的 LED 为低电平点亮，初始化为熄灭。
    let red_led = Output::new(p.PC14, Level::High, Speed::Low);
    let green_led = Output::new(p.PC13, Level::High, Speed::Low);

    // 开漏初始释放两个控制端，直到推杆任务开始回零才拉低 K2。
    let k1 = OutputOpenDrain::new(p.PB4, Level::High, Speed::Low);
    let k2 = OutputOpenDrain::new(p.PB5, Level::High, Speed::Low);
    let linear_actuator = LinearActuator::new(k1, k2);

    let uart_config = usart::Config::default(); // 115200、8N1
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
    let uart1 = init_uart!(spawner, uart1, 256);
    let adc1 = Adc::new(p.ADC1);
    let mut can = Can::new(p.CAN, p.PB8, p.PB9, Irqs); // 参数顺序是 RX、TX
    can.set_bitrate(500_000);
    can.modify_filters()
        .enable_bank(0, Fifo::Fifo0, Mask32::accept_all());
    Devices {
        red_led,
        green_led,
        uart1,
        adc1,
        adc_noise_pin: p.PA0,
        linear_actuator,
        can,
    }
}
