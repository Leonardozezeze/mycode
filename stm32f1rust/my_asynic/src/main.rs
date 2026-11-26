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
    
    
    

    loop {
        
    }
}
