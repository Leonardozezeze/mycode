//! Panic 行为：USART1 打印错误位置和消息，红灯（PF9）快闪。
//!
//! 最后防线路径，是分层契约的例外：直接用 PAC 裸指针操作寄存器。
//! 原因：panic 时任何所有权对象都不可信，也不能依赖尚未初始化的驱动。

use core::fmt::{self, Write};
use core::panic::PanicInfo;
use embassy_stm32::pac;

/// 内核主频（Hz）。若项目主频不同需同步修改，否则闪灯频率按比例变化。
const KERNEL_FREQUENCY: u32 = 168_000_000;
/// 闪烁半周期（毫秒）：红灯亮 HALF_PERIOD_MS、灭 HALF_PERIOD_MS。
const HALF_PERIOD_MS: u32 = 400;

struct PanicUart;

impl PanicUart {
    /// board::init 完成前 panic 时 USART1 未使能，跳过打印
    fn available() -> bool {
        let usart1 = pac::USART1;
        let cr1 = usart1.cr1().read();
        cr1.ue() && cr1.te()
    }
}

impl fmt::Write for PanicUart {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        let usart1 = pac::USART1;
        for b in s.bytes() {
            // 防空转：USART TX 异常时 txe 可能永不置位。
            // 约 100 万次自旋仍等不到就放弃打印，让调用方继续闪灯报警。
            let mut timeout = 1_000_000u32;
            while !usart1.sr().read().txe() {
                if timeout == 0 {
                    return Err(fmt::Error);
                }
                timeout -= 1;
                core::hint::spin_loop();
            }
            usart1.dr().write(|w| w.set_dr(b as u16));
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
/// 约束：单次 `cycles` 必须小于 2^32（168 MHz 下约 25.6 秒），
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
///
/// 若时钟尚未配到 KERNEL_FREQUENCY，闪灯频率会按比例变化，但不会卡死。
fn blink_red_forever() -> ! {
    let rcc = pac::RCC;
    let gpiof = pac::GPIOF;

    rcc.ahb1enr().modify(|w| w.set_gpiofen(true));
    cortex_m::asm::dsb(); // 时钟使能后按手册要求插屏障再访问外设
    gpiof
        .moder()
        .modify(|w| w.set_moder(9, pac::gpio::vals::Moder::OUTPUT));

    dwt_enable();

    // 用 u64 计算避免溢出；钳位到 u32::MAX 保证 wrapping_sub 比较有效。
    let half_period_cycles =
        ((KERNEL_FREQUENCY as u64) * (HALF_PERIOD_MS as u64) / 1000).min(u32::MAX as u64) as u32;

    loop {
        gpiof.bsrr().write(|w| w.set_bs(9, true)); // PF9 置位
        delay_cycles(half_period_cycles);
        gpiof.bsrr().write(|w| w.set_br(9, true)); // PF9 复位
        delay_cycles(half_period_cycles);
    }
}

#[panic_handler]
fn panic(info: &PanicInfo) -> ! {
    // panic 可能由中断引发，别让打印/闪灯被打断
    cortex_m::interrupt::disable();

    if PanicUart::available() {
        // 接管前停止 TX DMA，防止外设继续写 DR，与 panic 文本交错。
        pac::USART1.cr3().modify(|w| w.set_dmat(false));
        pac::DMA2.st(7).cr().modify(|w| w.set_en(false));
        // DMA 正在进行的总线事务结束后 EN 才清零；有界等待以免阻塞报警。
        for _ in 0..1_000_000 {
            if !pac::DMA2.st(7).cr().read().en() {
                break;
            }
            core::hint::spin_loop();
        }
        cortex_m::asm::dsb();
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
