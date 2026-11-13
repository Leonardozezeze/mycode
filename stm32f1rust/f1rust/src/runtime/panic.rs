//! Panic 行为：USART1 打印错误位置和消息，红灯（PC14）快闪。
//!
//! 最后防线路径，是分层契约的例外：直接用 PAC 裸指针操作寄存器。
//! 原因：panic 时任何所有权对象都不可信，也不能依赖尚未初始化的驱动。

use core::fmt::{self, Write};
use core::panic::PanicInfo;
use stm32f1xx_hal::pac;

/// 内核主频（Hz）。若项目主频不同需同步修改，否则闪灯频率按比例变化。
const KERNEL_FREQUENCY: u32 = 72_000_000;
/// 闪烁半周期（毫秒）：红灯亮 HALF_PERIOD_MS、灭 HALF_PERIOD_MS。
const HALF_PERIOD_MS: u32 = 400;

struct PanicUart;

impl PanicUart {
    /// board::init 完成前 panic 时 USART1 未使能，跳过打印
    fn available() -> bool {
        let usart1 = unsafe { &*pac::USART1::ptr() };
        let cr1 = usart1.cr1().read();
        cr1.ue().bit_is_set() && cr1.te().bit_is_set()
    }
}

impl fmt::Write for PanicUart {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        let usart1 = unsafe { &*pac::USART1::ptr() };
        for b in s.bytes() {
            // 防空转：USART TX 异常时 txe 可能永不置位。
            // 约 100 万次自旋仍等不到就放弃打印，让调用方继续闪灯报警。
            let mut timeout = 1_000_000u32;
            while usart1.sr().read().txe().bit_is_clear() {
                if timeout == 0 {
                    return Err(fmt::Error);
                }
                timeout -= 1;
                core::hint::spin_loop();
            }
            usart1.dr().write(|w| unsafe { w.bits(b as u16) });
        }
        Ok(())
    }
}

// ---- DWT CYCCNT 忙等延时 ----
// 不依赖 SysTick / HAL / 时基初始化，panic 场景下可直接使用。
// CYCCNT 由内核时钟直接驱动，只要内核在跑计数器就在跑。

const DEMCR: *mut u32 = 0xE000_EDFC as *mut u32;
const DWT_CTRL: *mut u32 = 0xE000_1000 as *mut u32;
const DWT_CYCCNT: *mut u32 = 0xE000_1004 as *mut u32;

/// 使能 DWT 周期计数器（幂等，多次调用无副作用）
fn dwt_enable() {
    unsafe {
        // 使能跟踪单元（TRCENA, bit 24）
        let demcr = core::ptr::read_volatile(DEMCR);
        if demcr & (1 << 24) == 0 {
            core::ptr::write_volatile(DEMCR, demcr | (1 << 24));
        }
        // 只在首次使能时清零计数器，避免干扰已在用 CYCCNT 的调用方
        let ctrl = core::ptr::read_volatile(DWT_CTRL);
        if ctrl & 1 == 0 {
            core::ptr::write_volatile(DWT_CYCCNT, 0);
            core::ptr::write_volatile(DWT_CTRL, ctrl | 1);
        }
    }
}

/// 忙等指定内核周期数。
///
/// 约束：单次 `cycles` 必须小于 2^32（72 MHz 下约 59.7 秒），
/// 否则 32 位计数器回绕会导致 `wrapping_sub` 比较失效。
/// 调用方负责钳位。
#[inline]
fn delay_cycles(cycles: u32) {
    unsafe {
        let start = core::ptr::read_volatile(DWT_CYCCNT);
        while core::ptr::read_volatile(DWT_CYCCNT).wrapping_sub(start) < cycles {
            core::hint::spin_loop();
        }
    }
}

/// 红灯快闪。用 DWT CYCCNT 做精确忙等：
/// - 不依赖时基初始化，任何状态下都能工作
/// - 延时精度为单个内核周期，可预测
/// 若时钟尚未配到 KERNEL_FREQUENCY，闪灯频率会按比例变化，但不会卡死。
fn blink_red_forever() -> ! {
    let rcc = unsafe { &*pac::RCC::ptr() };
    let gpioc = unsafe { &*pac::GPIOC::ptr() };

    rcc.apb2enr().modify(|_, w| w.iopcen().set_bit());
    cortex_m::asm::dsb(); // 时钟使能后按手册要求插屏障再访问外设
    // F1 CRH：PC14 的 MODE=10（2MHz），CNF=00（通用推挽）。
    gpioc
        .crh()
        .modify(|r, w| unsafe { w.bits((r.bits() & !(0xf << 24)) | (0x2 << 24)) });

    dwt_enable();

    // 用 u64 计算避免溢出；钳位到 u32::MAX 保证 wrapping_sub 比较有效。
    let half_period_cycles =
        ((KERNEL_FREQUENCY as u64) * (HALF_PERIOD_MS as u64) / 1000).min(u32::MAX as u64) as u32;

    loop {
        gpioc.bsrr().write(|w| unsafe { w.bits(1 << 14) }); // PC14 置位
        delay_cycles(half_period_cycles);
        gpioc.bsrr().write(|w| unsafe { w.bits(1 << (16 + 14)) }); // PC14 复位
        delay_cycles(half_period_cycles);
    }
}

#[panic_handler]
fn panic(info: &PanicInfo) -> ! {
    // panic 可能由中断引发，别让打印/闪灯被打断
    cortex_m::interrupt::disable();

    if PanicUart::available() {
        let mut w = PanicUart;
        // 忽略打印错误：串口异常时不让打印卡死闪灯流程
        let _ = write!(w, "\r\n==== PANIC ====\r\n");
        if let Some(loc) = info.location() {
            let _ = write!(w, "at {}:{}\r\n", loc.file(), loc.line());
        }
        let _ = write!(w, "{}\r\n", info.message());
    }

    blink_red_forever()
}
