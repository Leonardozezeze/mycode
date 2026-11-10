//! 阻塞延时 / 运行时间统计模块
//!
//! 基于 Cortex-M 内核的 DWT 周期计数器（CYCCNT）：
//! init 后随 HCLK 自由计数，`cycles()` 每次调用时把 32 位硬件计数
//! 软件折叠成 64 位累计值，对外提供单调不回绕的时间。

use core::sync::atomic::{AtomicU32, Ordering};
use cortex_m::peripheral::{self, DCB};

/// 上次读取到的 CYCCNT 值（用于检测回绕）
static LAST_CYCLES: AtomicU32 = AtomicU32::new(0);
/// CYCCNT 回绕次数（每次回绕代表又过了 2^32 个周期，168MHz 下约 25.6s）
static WRAP_COUNT: AtomicU32 = AtomicU32::new(0);
/// 每微秒的 CPU 周期数（init 时由 HCLK 推得）
static CYCLES_PER_US: AtomicU32 = AtomicU32::new(0);

pub struct DWT;

impl DWT {
    /// 使能周期计数器（上电调用一次，句柄由 `board::init()` 传入）。
    ///
    /// 计数从 0 开始，uptime 的零点即此刻；
    /// 句柄在此消耗，之后计数器自由运行，无需持有者。
    pub fn init(mut dcb: DCB, mut dwt: peripheral::DWT, hclk_hz: u32) {
        // DEMCR.TRCENA：不开全局 trace 的话，CYCCNT 使能位可能写不进去
        dcb.enable_trace();
        CYCLES_PER_US.store(hclk_hz / 1_000_000, Ordering::Relaxed);
        dwt.set_cycle_count(0);
        dwt.enable_cycle_counter();
    }
    /// 阻塞延时 `us` 微秒（单次上限约 25 秒 @168MHz）
    pub fn delay_us(us: u32) {
        let start = peripheral::DWT::cycle_count();
        let ticks = us.saturating_mul(CYCLES_PER_US.load(Ordering::Relaxed));
        // 无符号减法，天然处理 CYCCNT 溢出回绕
        while peripheral::DWT::cycle_count().wrapping_sub(start) < ticks {
            core::hint::spin_loop();
        }
    }
    /// 阻塞延时 `ms` 毫秒：逐毫秒累加，避免单次换算的周期数过大
    pub fn delay_ms(ms: u32) {
        for _ in 0..ms {
            Self::delay_us(1000);
        }
    }
    /// 自 `init()` 以来经过的 CPU 周期数（64 位，含回绕累加）
    ///
    /// 回绕是靠"两次调用之间计数器变小了"来发现的，
    /// 如果超过约 25.6 秒才调用一次，中间漏掉的回绕就补不回来了。
    pub fn cycles() -> u64 {
        // 放进临界区，避免主循环和中断同时更新回绕计数导致重复累加
        cortex_m::interrupt::free(|_| {
            let now = peripheral::DWT::cycle_count();
            let last = LAST_CYCLES.load(Ordering::Relaxed);
            let mut wraps = WRAP_COUNT.load(Ordering::Relaxed);

            if now < last {
                wraps = wraps.wrapping_add(1);
                WRAP_COUNT.store(wraps, Ordering::Relaxed);
            }
            LAST_CYCLES.store(now, Ordering::Relaxed);

            ((wraps as u64) << 32) | now as u64
        })
    }
    /// 自 `init()` 以来经过的微秒数
    pub fn uptime_us() -> u64 {
        Self::cycles() / CYCLES_PER_US.load(Ordering::Relaxed) as u64
    }
    /// 自 `init()` 以来经过的毫秒数
    pub fn uptime_ms() -> u64 {
        Self::uptime_us() / 1000
    }
}
