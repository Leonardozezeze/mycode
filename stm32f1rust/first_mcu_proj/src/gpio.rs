use crate::reg::*;

/// GPIO 模式枚举
pub enum GPIOMode {
    /// 输出模式，最大速度 2MHz
    OutputSpeed2Mhz,
    /// 输出模式，最大速度 10MHz
    OutputSpeed10Mhz,
    /// 输出模式，最大速度 50MHz
    OutputSpeed50Mhz,
    /// 输入模式
    Input,
}

/// GPIO 输出类型枚举
pub enum GPIOOutType {
    /// 推挽输出
    PushPull,
    /// 开漏输出
    OpenDrain,
    /// 复用推挽输出
    AfPushPull,
    /// 复用开漏输出
    AfOpenDrain,
}

/// GPIO 输入类型枚举
pub enum GPIOInType {
    /// 模拟输入
    Analog,
    /// 浮空输入
    Floating,
    /// 上下拉输入
    PullUpDown,
}

/// GPIO 上下拉选择枚举
pub enum GPIOInUd {
    /// 上拉
    Up,
    /// 下拉
    Down,
}

/// GPIO 引脚状态枚举
pub enum PinState {
    /// 高电平
    High,
    /// 低电平
    Low,
}

/// 设置 GPIO 引脚模式
///
/// # Safety
/// 直接操作硬件寄存器，需确保 port 地址有效
pub unsafe fn set_gpio_mode(port: u32, pin: u8, mode: GPIOMode) {
    assert!(pin < 16, "make sure pin between 0-15");
    let (offset, bit_pos) = if pin < 8 { (0, pin * 4) } else { (0x04, (pin - 8) * 4) };
    let gpio_mode_reg_addr = (port + offset) as *mut u32;
    let val = match mode {
        GPIOMode::OutputSpeed50Mhz => 0x3u32 << bit_pos,
        GPIOMode::OutputSpeed10Mhz => 0x1u32 << bit_pos,
        GPIOMode::OutputSpeed2Mhz => 0x2u32 << bit_pos,
        GPIOMode::Input => 0x0u32 << bit_pos,
    };
    unsafe {
        reg_write_mask(gpio_mode_reg_addr, val, 0x3u32 << bit_pos);
    }
}

/// 设置 GPIO 输出类型
///
/// # Safety
/// 直接操作硬件寄存器，需确保 port 地址有效
pub unsafe fn set_gpio_out_type(port: u32, pin: u8, outtype: GPIOOutType) {
    assert!(pin < 16, "make sure pin between 0-15");
    let (offset, bit_pos) = if pin < 8 { (0, pin * 4) } else { (0x04, (pin - 8) * 4) };
    let gpio_mode_reg_addr = (port + offset) as *mut u32;
    let val = match outtype {
        GPIOOutType::AfOpenDrain => 0x3u32 << (bit_pos + 2),
        GPIOOutType::OpenDrain => 0x1u32 << (bit_pos + 2),
        GPIOOutType::AfPushPull => 0x2u32 << (bit_pos + 2),
        GPIOOutType::PushPull => 0x0u32 << (bit_pos + 2),
    };
    unsafe {
        reg_write_mask(gpio_mode_reg_addr, val, 0x3u32 << (bit_pos + 2));
    }
}

/// 设置 GPIO 输入类型
///
/// # Safety
/// 直接操作硬件寄存器，需确保 port 地址有效
pub unsafe fn set_gpio_in_type(port: u32, pin: u8, intype: GPIOInType) {
    assert!(pin < 16, "make sure pin between 0-15");
    let (offset, bit_pos) = if pin < 8 { (0, pin * 4) } else { (0x04, (pin - 8) * 4) };
    let gpio_mode_reg_addr = (port + offset) as *mut u32;
    let val = match intype {
        GPIOInType::Floating => 0x1u32 << (bit_pos + 2),
        GPIOInType::PullUpDown => 0x2u32 << (bit_pos + 2),
        GPIOInType::Analog => 0x0u32 << (bit_pos + 2),
    };
    unsafe {
        reg_write_mask(gpio_mode_reg_addr, val, 0x3u32 << (bit_pos + 2));
    }
}

/// 设置 GPIO 上下拉
///
/// # Safety
/// 直接操作硬件寄存器，需确保 port 地址有效
pub unsafe fn set_gpio_in_ud(port: u32, pin: u8, ud: GPIOInUd) {
    assert!(pin < 16, "make sure pin between 0-15");
    let offset = 0xC;
    let gpio_mode_reg_addr = (port + offset) as *mut u32;
    match ud {
        GPIOInUd::Up => unsafe {
            reg_set_bits(gpio_mode_reg_addr, pin);
        },
        GPIOInUd::Down => unsafe {
            reg_unset_bits(gpio_mode_reg_addr, pin);
        },
    };
}

/// 设置 GPIO 引脚输出高电平
///
/// # Safety
/// 直接操作硬件寄存器，需确保 port 地址有效
pub unsafe fn gpio_set_bits(port: u32, pin: u8) {
    assert!(pin < 16, "make sure pin between 0-15");
    unsafe {
        reg_set_bits((port + 0xC) as *mut u32, pin);
    }
}

/// 设置 GPIO 引脚输出低电平
///
/// # Safety
/// 直接操作硬件寄存器，需确保 port 地址有效
pub unsafe fn gpio_reset_bits(port: u32, pin: u8) {
    assert!(pin < 16, "make sure pin between 0-15");
    unsafe {
        reg_unset_bits((port + 0xC) as *mut u32, pin);
    }
}

/// 写入 GPIO 引脚状态
///
/// # Safety
/// 直接操作硬件寄存器，需确保 port 地址有效
pub unsafe fn gpio_write_pin(port: u32, pin: u8, state: PinState) {
    assert!(pin < 16, "make sure pin between 0-15");
    match state {
        PinState::High => unsafe { gpio_set_bits(port, pin) },
        PinState::Low => unsafe { gpio_reset_bits(port, pin) },
    }
}

/// 读取 GPIO 引脚状态
///
/// # Safety
/// 直接操作硬件寄存器，需确保 port 地址有效
pub unsafe fn gpio_read_pin(port: u32, pin: u8) -> PinState {
    assert!(pin < 16, "make sure pin between 0-15");
    let val = unsafe { read_register((port + 0x08) as *mut u32) };
    match (val >> pin) & 1 {
        1 => PinState::High,
        _ => PinState::Low,
    }
}
