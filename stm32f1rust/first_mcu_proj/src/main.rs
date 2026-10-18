#![no_std]
#![no_main]
#![allow(clippy::empty_loop)]

use core::panic::PanicInfo;

#[unsafe(no_mangle)] 
fn main()->! {

    loop {
        
    }
}

#[panic_handler]
fn panic_handler(_info:&PanicInfo)->! {
    loop {
        
    }
}
