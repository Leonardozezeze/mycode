//! TIM4 正交编码器和异步计数采样。

use embassy_stm32::{
    Peri,
    peripherals::TIM4,
    timer::{
        Ch1, Ch2, TimerPin,
        qei::{Config, Qei, QeiMode},
    },
};
use embassy_time::{Duration, Instant, Timer};

#[derive(Debug)]
pub struct EncoderSample {
    /// 两次读取之间的带方向净计数。
    pub counts: i16,
    /// 两次读取之间的实际时间，包含任务恢复的延迟。
    pub elapsed: Duration,
}

#[derive(Clone, Copy)]
pub struct EncoderSnapshot {
    pub timestamp: Instant,
    count: u16,
}

impl EncoderSnapshot {
    /// 该次采样读取的 16 位硬件计数。
    pub fn count(self) -> u16 {
        self.count
    }

    /// 当前读数须晚于起点。实际净计数的绝对值须小于 32768。
    pub fn sample_since(self, start: Self) -> EncoderSample {
        EncoderSample {
            counts: self.count.wrapping_sub(start.count) as i16,
            elapsed: self.timestamp.duration_since(start.timestamp),
        }
    }
}

pub struct Encoder<'d> {
    qei: Qei<'d, TIM4>,
}

impl<'d> Encoder<'d> {
    pub fn new(
        tim: Peri<'d, TIM4>,
        a: Peri<'d, impl TimerPin<TIM4, Ch1>>,
        b: Peri<'d, impl TimerPin<TIM4, Ch2>>,
    ) -> Self {
        let config = Config {
            mode: QeiMode::Mode3, // A/B 两相的上升沿和下降沿均计数（×4）。
            auto_reload: u16::MAX,
            ..Config::default()
        };
        Self {
            qei: Qei::new(tim, a, b, config),
        }
    }

    /// 读取持续运行的 16 位硬件计数器。
    pub fn count(&self) -> u16 {
        self.qei.count()
    }

    /// 同时记录计数及其时间，供连续采样复用起点。
    pub fn snapshot(&self) -> EncoderSnapshot {
        EncoderSnapshot {
            count: self.count(),
            timestamp: Instant::now(),
        }
    }

    /// 采样并只返回净计数。测速时用 [`Self::sample_for`] 获取实际间隔。
    pub async fn count_for(&self, window: Duration) -> i16 {
        self.sample_for(window).await.counts
    }

    /// 等到采样任务恢复后读取计数，并报告实际采样间隔。
    /// 实际窗口内净计数的绝对值须小于 32768，才能从 16 位回绕中恢复方向。
    pub async fn sample_for(&self, window: Duration) -> EncoderSample {
        if window.as_ticks() == 0 {
            return EncoderSample {
                counts: 0,
                elapsed: Duration::MIN,
            };
        }

        let start = self.snapshot();
        Timer::at(start.timestamp + window).await;
        self.snapshot().sample_since(start)
    }
}
