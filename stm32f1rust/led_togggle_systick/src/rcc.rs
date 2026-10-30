use crate::reg::*;

/// RCC 基地址
const RCC_BASE: u32 = 0x4002_1000;

/// APB2 外设时钟使能寄存器偏移
const RCC_APB2ENR_OFFSET: u32 = 0x18;

/// GPIO 时钟使能枚举
pub enum GPIOClk {
    /// GPIOA 时钟
    GPIOA,
    /// GPIOB 时钟
    GPIOB,
    /// GPIOC 时钟
    GPIOC,
    /// GPIOD 时钟
    GPIOD,
    /// GPIOE 时钟
    GPIOE,
}

/// 使能 GPIO 时钟
///
/// # Safety
/// 直接操作硬件寄存器
pub unsafe fn gpio_clk_enable(gpio: GPIOClk) {
    let rcc_apb2enr = (RCC_BASE + RCC_APB2ENR_OFFSET) as *mut u32;
    let bit_pos = match gpio {
        GPIOClk::GPIOA => 2,
        GPIOClk::GPIOB => 3,
        GPIOClk::GPIOC => 4,
        GPIOClk::GPIOD => 5,
        GPIOClk::GPIOE => 6,
    };
    unsafe {
        reg_set_bits(rcc_apb2enr, bit_pos);
    }
}

/// 禁用 GPIO 时钟
///
/// # Safety
/// 直接操作硬件寄存器
pub unsafe fn gpio_clk_disable(gpio: GPIOClk) {
    let rcc_apb2enr = (RCC_BASE + RCC_APB2ENR_OFFSET) as *mut u32;
    let bit_pos = match gpio {
        GPIOClk::GPIOA => 2,
        GPIOClk::GPIOB => 3,
        GPIOClk::GPIOC => 4,
        GPIOClk::GPIOD => 5,
        GPIOClk::GPIOE => 6,
    };
    unsafe {
        reg_unset_bits(rcc_apb2enr, bit_pos);
    }
}
