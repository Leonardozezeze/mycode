#![no_main]
#![no_std]

use cortex_m_rt::entry;
use panic_probe as _;
use defmt_rtt as _; 
use defmt::println;
use stm32f4xx_hal::{pac, prelude::*,rcc::Config};

#[entry]
fn main() -> ! {
    let dp = pac::Peripherals::take().unwrap();
    let cp = cortex_m::peripheral::Peripherals::take().unwrap();

    // 配置时钟：外部 HSE 8MHz，PLL 到 168MHz
    let mut rcc = dp.RCC.freeze(Config::hse(8.MHz()).sysclk(168.MHz()));

    // LED: PF9（STM32F4-Discovery 板载 LED）
    let gpiof = dp.GPIOF.split(&mut rcc);
    let mut led = gpiof.pf9.into_push_pull_output();

    // SysTick 延时
    let mut delay = cp.SYST.delay(&rcc.clocks);

    println!("STM32F407 booted!");

    loop {
        led.toggle();
        delay.delay_ms(500u32);
    }
}
