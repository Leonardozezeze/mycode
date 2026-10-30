#![no_std]
#![no_main]
#![allow(dead_code)]
use cortex_m_rt::{
    entry,
    exception,
};
use cortex_m::{
    peripheral::Peripherals,
    peripheral::syst,
};
use panic_halt as _;

use crate::board::{BLUE_LED_PIN, BLUE_LED_PORT};

mod led;
mod gpio;
mod mcu;
mod reg;
mod board;
mod rcc;
#[entry]
fn main()->! {
    led::led_init(BLUE_LED_PORT, BLUE_LED_PIN);
    led::led_on(BLUE_LED_PORT, BLUE_LED_PIN);
    let mut peripherals=Peripherals::take().unwrap();
    let systick=&mut peripherals.SYST;
    systick.set_clock_source(syst::SystClkSource::Core);
    systick.set_reload(4_000_000);
    systick.clear_current();
    systick.enable_interrupt();
    systick.enable_counter();

    loop {
        cortex_m::asm::wfi();
    }
}

#[exception]
fn SysTick(){
    led::led_toggle(BLUE_LED_PORT, BLUE_LED_PIN);
}
