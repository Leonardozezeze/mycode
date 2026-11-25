//! Embassy 调度延时示例：PB12 输出一次 500 ms 低电平脉冲，PB13 保持释放。
//! 开漏波形测试时，两个引脚各用约 10 kΩ 电阻上拉到 3.3 V。
//! 先用逻辑分析仪观测测试引脚；接驱动板前确认输入电压、电流及接口电路。
//! 编译：cargo build --example actuator_async --offline
//! 这是单独的示例程序，运行它会在启动后执行一次动作。

#![no_std]
#![no_main]
#![allow(dead_code)]

#[path = "../src/drivers/liner_actuator.rs"]
mod liner_actuator;

use embassy_executor::Spawner;
use embassy_stm32::{
    gpio::{Level, OutputOpenDrain, Speed},
    rcc::{AHBPrescaler, APBPrescaler, Hse, HseMode, Pll, PllMul, PllPreDiv, PllSource, Sysclk},
    time::Hertz,
};
use liner_actuator::LinearActuator;

#[embassy_executor::main]
async fn main(_spawner: Spawner) {
    // 与主工程相同：8 MHz 晶振，72 MHz 系统时钟。
    let mut config = embassy_stm32::Config::default();
    config.rcc.hse = Some(Hse {
        freq: Hertz(8_000_000),
        mode: HseMode::Oscillator,
    });
    config.rcc.pll = Some(Pll {
        src: PllSource::HSE,
        prediv: PllPreDiv::DIV1,
        mul: PllMul::MUL9,
    });
    config.rcc.sys = Sysclk::PLL1_P;
    config.rcc.ahb_pre = AHBPrescaler::DIV1;
    config.rcc.apb1_pre = APBPrescaler::DIV2;
    config.rcc.apb2_pre = APBPrescaler::DIV1;
    let p = embassy_stm32::init(config);

    // 测试引脚可换成任意空闲 GPIO；调度延时方案不要求使用定时器复用引脚。
    let k1 = OutputOpenDrain::new(p.PB12, Level::High, Speed::Low);
    let k2 = OutputOpenDrain::new(p.PB13, Level::High, Speed::Low);
    let mut actuator = LinearActuator::new(k1, k2);

    // 本任务异步等待约 500 ms，其他任务可运行；返回时 K1 已释放。
    actuator.extend_for(500).await.unwrap();

    // 其他调用示例：
    // actuator.retract_for(250).await.unwrap();
    // actuator.extend_mm(6.0).await.unwrap(); // 24 mm/s 下约 250 ms
    // 确认实际位置后才能登记，例如确认完全缩回后：
    // actuator.set_position_mm(0.0).unwrap();
    // actuator.move_to(10.0).await.unwrap(); // 估算移到伸出 10 mm 的位置
    // actuator.stop();
    core::future::pending::<()>().await;
}

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo<'_>) -> ! {
    cortex_m::interrupt::disable();
    // 示例的两个测试输出先释放，再停住。更改测试引脚时同步更改此处。
    embassy_stm32::pac::GPIOB.bsrr().write(|w| {
        w.set_bs(12, true);
        w.set_bs(13, true);
    });
    loop {
        cortex_m::asm::wfi();
    }
}
