#![no_std]
#![no_main]
#![allow(dead_code)]
#![allow(clippy::empty_loop)]

use core::panic::PanicInfo;
use crate::board::BLUE_LED_PIN;
use crate::board::BLUE_LED_PORT;
use crate::led::*;
mod start;
mod mcu;
mod reg;
mod gpio;
mod rcc;
mod board;
mod led;
mod button;

#[unsafe(no_mangle)]
fn main()->! {
    led_init(BLUE_LED_PORT,BLUE_LED_PIN);
    loop {
        led_on(BLUE_LED_PORT, BLUE_LED_PIN);
        // 简单延时
        for _ in 0..100000 {
            core::hint::spin_loop();
        }
        led_off(BLUE_LED_PORT, BLUE_LED_PIN);
        for _ in 0..100000 {
            core::hint::spin_loop();
        }
    }
}

#[panic_handler]
fn panic_handler(_info:&PanicInfo)->! {
    loop {
        
    }
}

// fn EXTI0_Handler(){
//     led_toggle(BLUE_LED_PORT,BLUE_LED_PIN);
// }
