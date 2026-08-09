#![no_main]
#![no_std]

use panic_halt as _;
use cortex_m_rt::entry;
use stm32f4xx_hal::{
    pac, prelude::*, rcc::Config,
};

#[entry]
fn main() -> ! {
    let dp = pac::Peripherals::take().unwrap();
    let cp = cortex_m::peripheral::Peripherals::take().unwrap();
    let mut rcc = dp.RCC.freeze(Config::hse(8.MHz()).sysclk(168.MHz()));
    let mut delay = cp.SYST.delay(&rcc.clocks);


    let gpiof = dp.GPIOF.split(&mut rcc);
    let mut led = gpiof.pf9.into_push_pull_output();

    loop {
        led.toggle();
        delay.delay_ms(500u32);
    }
}
