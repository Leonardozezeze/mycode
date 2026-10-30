use crate::{gpio::*, rcc::*, mcu::*};
pub fn led_init(port: u32, pin: u8) {
    unsafe {
        // 根据 port 地址使能对应的 GPIO 时钟
        match port {
            GPIOA => gpio_clk_enable(GPIOClk::GPIOA),
            GPIOB => gpio_clk_enable(GPIOClk::GPIOB),
            GPIOC => gpio_clk_enable(GPIOClk::GPIOC),
            GPIOD => gpio_clk_enable(GPIOClk::GPIOD),
            _ => {}
        }
        //output mode
        set_gpio_mode(port, pin, GPIOMode::OutputSpeed2Mhz);
        //output type
        set_gpio_out_type(port, pin, GPIOOutType::PushPull);
    }
}

pub fn led_on(port: u32, pin: u8) {
    unsafe {
        gpio_reset_bits(port, pin);  // PC13 LED 低电平点亮
    }
}

pub fn led_off(port: u32, pin: u8) {
    unsafe {
        gpio_set_bits(port, pin);  // PC13 LED 高电平熄灭
    }
}
pub fn led_toggle(port: u32, pin: u8) {
    unsafe {
        match gpio_read_pin(port, pin) {
            PinState::High => led_off(port, pin),
            PinState::Low => led_on(port, pin),
        }
    }
}
