#![no_std]
#![no_main]

use panic_halt as _;
use stm32f1xx_hal as hal;

use cortex_m_rt::entry;
use hal::prelude::*;
use hal::timer::SysTimerExt;

#[entry]
fn main() -> ! {
    let dp = hal::pac::Peripherals::take().unwrap();
    let cp = cortex_m::Peripherals::take().unwrap();
    let mut rcc = dp.RCC.constrain();
    let mut gpioc = dp.GPIOC.split(&mut rcc);
    let mut led = gpioc.pc13.into_push_pull_output(&mut gpioc.crh);

    let mut delay = cp.SYST.delay(&rcc.clocks);

    loop {
        led.set_high();
        delay.delay(1.secs());
        led.set_low();
        delay.delay(1.secs());
    }
}
