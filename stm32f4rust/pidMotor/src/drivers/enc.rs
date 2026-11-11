//! 16 位定时器编码器封装：原始计数和带符号计数差，不负责计时与测速。

use stm32f4xx_hal::{
    hal_02::Qei as _,
    qei::{Instance, Qei},
};

/// 适用于 TIM4 等 16 位编码器定时器。
pub struct Encoder<TIM: Instance<Width = u16>> {
    qei: Qei<TIM>,
    last_count: u16,
}

impl<TIM: Instance<Width = u16>> Encoder<TIM> {
    /// 接收 board 配置好的 QEI，以当前计数作为计数差基准。
    pub fn new(qei: Qei<TIM>) -> Self {
        let last_count = qei.count();
        Self { qei, last_count }
    }

    /// 读取硬件原始计数，不改变测速基准。
    pub fn count(&self) -> u16 {
        self.qei.count()
    }

    /// 读取距上次调用的带符号计数差，并更新基准；不会清零硬件计数器。
    /// 正负号表示计数增减方向，实际机械方向取决于 AB 相接线。
    /// 两次调用间实际位移必须在 (-32768, 32768) 计数内，否则无法判定回绕。
    /// 应由一个采样任务调用，再将采样结果共享给其他业务。
    pub fn delta_count(&mut self) -> i16 {
        let count = self.qei.count();
        let delta = count_delta(count, self.last_count);
        self.last_count = count;
        delta
    }
}

/// 在相邻采样位移不足半圈计数范围时，还原带方向的位移。
fn count_delta(current: u16, previous: u16) -> i16 {
    current.wrapping_sub(previous) as i16
}
