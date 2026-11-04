//! 阻塞延时 / 运行时间统计模块
//!
//! 直接使用 Cortex-M 内核的 DWT 周期计数器（CYCCNT），不再经过 C FFI。

use core::sync::atomic::{AtomicU32, Ordering};

/// CPU 主频（HCLK），需与 C 侧 SystemClock_Config 一致：
/// HSE 8MHz / PLLM 4 * PLLN 168 / PLLP 2 = 168MHz
const CPU_FREQ_HZ: u32 = 168_000_000;

/// 每微秒的 CPU 周期数
const CYCLES_PER_US: u32 = CPU_FREQ_HZ / 1_000_000;

/// 上次读取到的 CYCCNT 值（用于检测回绕）
static LAST_CYCLES: AtomicU32 = AtomicU32::new(0);
/// CYCCNT 回绕次数（每次回绕代表又过了 2^32 个周期，约 25.6s）
static WRAP_COUNT: AtomicU32 = AtomicU32::new(0);

pub struct DWT;

impl DWT {
    /// 使能 DWT 周期计数器（上电后调用一次）
    ///
    /// 会取走 `cortex_m::Peripherals`，因此只能调用一次。
    pub fn init() {
        let mut cp = cortex_m::Peripherals::take().expect("core peripherals already taken");
        // DEMCR.TRCENA：不开全局 trace 的话，CYCCNT 使能位可能写不进去
        cp.DCB.enable_trace();
        // 从 0 开始计数，让 uptime 的零点就是这里的调用时刻
        cp.DWT.set_cycle_count(0);
        cp.DWT.enable_cycle_counter();
    }
    /// 阻塞延时 `us` 微秒（单次上限约 25 秒 @168MHz）
    pub fn delay_us(us: u32) {
        let start = cortex_m::peripheral::DWT::cycle_count();
        let ticks = us.saturating_mul(CYCLES_PER_US);
        // 无符号减法，天然处理 CYCCNT 溢出回绕
        while cortex_m::peripheral::DWT::cycle_count().wrapping_sub(start) < ticks {
            core::hint::spin_loop();
        }
    }
    /// 阻塞延时 `ms` 毫秒：逐毫秒累加，避免单次换算的周期数过大
    pub fn delay_ms(ms: u32) {
        for _ in 0..ms {
            Self::delay_us(1000);
        }
    }
    /// 自 `DWT::init()` 以来经过的 CPU 周期数（64 位，含回绕累加）
    ///
    /// 注意：回绕是靠"两次调用之间计数器变小了"来发现的，
    /// 如果超过约 25.6 秒才调用一次，中间漏掉的回绕就补不回来了。
    pub fn cycles() -> u64 {
        // 放进临界区，避免主循环和中断同时更新回绕计数导致重复累加
        cortex_m::interrupt::free(|_| {
            let now = cortex_m::peripheral::DWT::cycle_count();
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
    /// 自 `DWT::init()` 以来经过的微秒数
    pub fn uptime_us() -> u64 {
        Self::cycles() / CYCLES_PER_US as u64
    }
    /// 自 `DWT::init()` 以来经过的毫秒数
    pub fn uptime_ms() -> u64 {
        Self::uptime_us() / 1000
    }
}
