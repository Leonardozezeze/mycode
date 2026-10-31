//! 主机端单元测试（不依赖硬件）
//!
//! 运行：
//!
//! ```bash
//! cargo test-host    # = cargo test --target x86_64-unknown-linux-gnu
//! ```
//!
//! 注意：
//! - 不要在这里调用 `HAL_xxx` 等 FFI，它们指向 MCU 寄存器，在主机上会段错误。
//! - 算法验证请先实现成纯函数，再在这里写输入和断言。

use crate::hal::gpio::ffi;

/// GPIO_PIN_x 必须是从 bit0 开始、每个引脚一位的掩码
#[test]
fn gpio_pin_masks_are_single_bits() {
    let pins = [
        ffi::GPIO_PIN_0,
        ffi::GPIO_PIN_1,
        ffi::GPIO_PIN_2,
        ffi::GPIO_PIN_3,
        ffi::GPIO_PIN_4,
        ffi::GPIO_PIN_5,
        ffi::GPIO_PIN_6,
        ffi::GPIO_PIN_7,
        ffi::GPIO_PIN_8,
        ffi::GPIO_PIN_9,
        ffi::GPIO_PIN_10,
        ffi::GPIO_PIN_11,
        ffi::GPIO_PIN_12,
        ffi::GPIO_PIN_13,
        ffi::GPIO_PIN_14,
        ffi::GPIO_PIN_15,
    ];

    for (bit, pin) in pins.iter().enumerate() {
        assert_eq!(*pin, 1u32 << bit, "GPIO_PIN_{bit} = {pin:#x}");
    }
}

/// GPIOA~GPIOD 基地址相差 0x400（STM32F1 GPIO 外设布局）
#[test]
fn gpio_port_addresses_are_contiguous() {
    let ports = [ffi::GPIOA, ffi::GPIOB, ffi::GPIOC, ffi::GPIOD];

    for (index, port) in ports.iter().enumerate() {
        assert_eq!(
            *port as usize,
            0x4001_0800 + index * 0x400,
            "GPIO{index} 基地址不对"
        );
    }
}

// ============================================================
// 算法验证（占位）
//
// 具体算法待补充，直接在函数体里写输入、调用和 assert。
// ============================================================

#[test]
fn algorithm_test_1() {
    // TODO
}

#[test]
fn algorithm_test_2() {
    // TODO
}

#[test]
fn algorithm_edge_cases() {
    // TODO
}
