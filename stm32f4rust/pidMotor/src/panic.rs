//! Panic 行为：USART1 打印错误位置和消息，红灯（PF9）快闪。
//!
//! 最后防线路径，是分层契约的例外：直接用 PAC 裸指针操作寄存器。
//! 原因：panic 时任何所有权对象都不可信，也不能依赖尚未初始化的驱动。

use core::fmt::{self, Write};
use core::panic::PanicInfo;
use stm32f4xx_hal::pac;

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
            while usart1.sr().read().txe().bit_is_clear() {}
            usart1.dr().write(|w| unsafe { w.bits(b as u16) });
        }
        Ok(())
    }
}

/// 红灯快闪。不用 DWT::delay_ms：时基未初始化时周期计数器没在跑，
/// 会永远等不到；傻循环空转在任何状态下都能工作。
fn blink_red_forever() -> ! {
    let rcc = unsafe { &*pac::RCC::ptr() };
    let gpiof = unsafe { &*pac::GPIOF::ptr() };

    rcc.ahb1enr().modify(|_, w| w.gpiofen().enabled());
    cortex_m::asm::dsb(); // 时钟使能后按手册要求插屏障再访问外设
    gpiof.moder().modify(|_, w| w.moder9().output());

    loop {
        gpiof.bsrr().write(|w| unsafe { w.bits(1 << 9) }); // PF9 置位
        for _ in 0..1_000_000 {
            core::hint::spin_loop()
        }
        gpiof.bsrr().write(|w| unsafe { w.bits(1 << (16 + 9)) }); // PF9 复位
        for _ in 0..1_000_000 {
            core::hint::spin_loop()
        }
    }
}

#[panic_handler]
fn panic(info: &PanicInfo) -> ! {
    // panic 可能由中断引发，别让打印/闪灯被打断
    cortex_m::interrupt::disable();

    if PanicUart::available() {
        let mut w = PanicUart;
        let _ = write!(w, "\r\n==== PANIC ====\r\n");
        if let Some(loc) = info.location() {
            let _ = write!(w, "at {}:{}\r\n", loc.file(), loc.line());
        }
        let _ = write!(w, "{}\r\n", info.message());
    }

    blink_red_forever()
}
