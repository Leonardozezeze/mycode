embassy_stm32::bind_interrupts!(struct Irqs{
    USART1=>embassy_stm32::usart::InterruptHandler<embassy_stm32::peripherals::USART1>;
    DMA1_CHANNEL4=>embassy_stm32::dma::InterruptHandler<embassy_stm32::peripherals::DMA1_CH4>;
    DMA1_CHANNEL5=>embassy_stm32::dma::InterruptHandler<embassy_stm32::peripherals::DMA1_CH5>;
});

pub struct Devices {
    pub red_led: embassy_stm32::gpio::Output<'static>,
    pub green_led: embassy_stm32::gpio::Output<'static>,
    pub tx: embassy_stm32::usart::UartTx<'static, embassy_stm32::mode::Async>,
    pub rx: embassy_stm32::usart::RingBufferedUartRx<'static>,
}

pub fn init() -> Devices {
    let mut config=embassy_stm32::Config::default();
    config.rcc.hse=Some(embassy_stm32::rcc::Hse{
        freq:embassy_stm32::time::Hertz(8_000_000),
        mode:embassy_stm32::rcc::HseMode::Oscillator,
    });
    config.rcc.pll=Some(embassy_stm32::rcc::Pll{
        src:embassy_stm32::rcc::PllSource::HSE,
        prediv:embassy_stm32::rcc::PllPreDiv::DIV1,
        mul:embassy_stm32::rcc::PllMul::MUL9,
    });
    config.rcc.sys=embassy_stm32::rcc::Sysclk::PLL1_P;
    config.rcc.ahb_pre=embassy_stm32::rcc::AHBPrescaler::DIV1;
    config.rcc.apb1_pre=embassy_stm32::rcc::APBPrescaler::DIV2;
    config.rcc.apb2_pre=embassy_stm32::rcc::APBPrescaler::DIV1;

    let p=embassy_stm32::init(config);//初始化配置并获得外设句柄
    let green_led=embassy_stm32::gpio::Output::new(
        p.PC13,
        embassy_stm32::gpio::Level::High,
        embassy_stm32::gpio::Speed::Low,
    );
    let red_led=embassy_stm32::gpio::Output::new(
        p.PC14, 
        embassy_stm32::gpio::Level::High, 
    embassy_stm32::gpio::Speed::Low,
    );
    let uart_config=embassy_stm32::usart::Config::default();//生成uart底层配置
    let uart1=embassy_stm32::usart::Uart::new(
        p.USART1, 
        p.PA10, 
        p.PA9, 
        p.DMA1_CH4, 
        p.DMA1_CH5, Irqs, 
        uart_config,
    ).expect("invalid USART1 config!");
    let (tx,rx)=uart1.split();
    static RX_BUFFER:static_cell::StaticCell<[u8;256]>=static_cell::StaticCell::new();
    let mut rx=rx.into_ring_buffered(RX_BUFFER.init([0;256]));
    rx.start_uart();
    Devices {
    red_led,
    green_led,
    tx,
    rx,
    }
}
