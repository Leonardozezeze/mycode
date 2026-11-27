//! 板级支持：时钟、引脚、DMA 和中断绑定集中在这里。
//! TIM9 由 embassy-time 独占，TIM3 输出电机 PWM，TIM4 读取编码器。
//! TIM9 共用 TIM1_BRK_TIM9 中断，后续使用 TIM1 刹车功能时需协调中断处理。

use super::config::{
    can::CAN1_BITRATE,
    motor::MOTOR_PWM_TICK_HZ,
    uart::{UART1_BAUD_RATE, UART1_RX_DMA_BUFFER_BYTES},
};
use crate::drivers::{
    encoder::Encoder,
    liner_actuator::LinearActuator,
    tb6612::Tb6612,
    uart::{UartLink, init_uart},
};
use embassy_executor::Spawner;
use embassy_stm32::{
    bind_interrupts,
    can::{self, Can, Fifo, filter::Mask32},
    dma,
    gpio::{Level, Output, OutputOpenDrain, OutputType, Speed},
    peripherals,
    rcc::{
        AHBPrescaler, APBPrescaler, Hse, HseMode, Pll, PllMul, PllPDiv, PllPreDiv, PllQDiv,
        PllSource, Sysclk,
    },
    time::Hertz,
    timer::simple_pwm::PwmPin,
    usart::{self, Uart},
};

bind_interrupts!(struct Irqs {
    USART1 => usart::InterruptHandler<peripherals::USART1>;
    DMA2_STREAM7 => dma::InterruptHandler<peripherals::DMA2_CH7>;
    DMA2_STREAM2 => dma::InterruptHandler<peripherals::DMA2_CH2>;
    CAN1_TX => can::TxInterruptHandler<peripherals::CAN1>;
    CAN1_RX0 => can::Rx0InterruptHandler<peripherals::CAN1>;
    CAN1_RX1 => can::Rx1InterruptHandler<peripherals::CAN1>;
    CAN1_SCE => can::SceInterruptHandler<peripherals::CAN1>;
});

pub struct Devices {
    pub red_led: Output<'static>,
    pub green_led: Output<'static>,
    /// USART1 用于调试日志输出和调试数据接收。
    pub uart1: &'static UartLink,
    pub can: Can<'static>,
    pub motor: Tb6612<'static, peripherals::TIM3>,
    pub encoder: Encoder<'static>,
    pub actuator: LinearActuator,
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

    // PC2 -> K1，PC3 -> K2，低电平有效。上电时释放两路开漏输出。
    let k1 = OutputOpenDrain::new(p.PC2, Level::High, Speed::Low);
    let k2 = OutputOpenDrain::new(p.PC3, Level::High, Speed::Low);
    let actuator = LinearActuator::new(k1, k2);

    // PF9/PF10 的 LED 为低电平点亮，初始化为熄灭。
    let red_led = Output::new(p.PF9, Level::High, Speed::Low);
    let green_led = Output::new(p.PF10, Level::High, Speed::Low);

    let mut uart_config = usart::Config::default(); // 8N1
    uart_config.baudrate = UART1_BAUD_RATE;
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
    let uart1 = init_uart!(spawner, uart1, UART1_RX_DMA_BUFFER_BYTES);

    let mut can = Can::new(p.CAN1, p.PA11, p.PA12, Irqs); // 参数顺序是 RX、TX
    can.set_bitrate(CAN1_BITRATE);
    can.modify_filters()
        .enable_bank(0, Fifo::Fifo0, Mask32::accept_all());

    // 沿用旧板接线：PA6 -> PWMA，PA4 -> AIN1，PA5 -> AIN2，STBY 外部拉高。
    let ain1 = Output::new(p.PA4, Level::Low, Speed::Low);
    let ain2 = Output::new(p.PA5, Level::Low, Speed::Low);
    let pwma = PwmPin::new(p.PA6, OutputType::PushPull);
    let motor = Tb6612::new(p.TIM3, pwma, ain1, ain2, Hertz(MOTOR_PWM_TICK_HZ));

    // 沿用旧板接线：PD12/TIM4_CH1 接 A 相，PD13/TIM4_CH2 接 B 相。
    let encoder = Encoder::new(p.TIM4, p.PD12, p.PD13);

    Devices {
        red_led,
        green_led,
        uart1,
        can,
        motor,
        encoder,
        actuator,
    }
}
