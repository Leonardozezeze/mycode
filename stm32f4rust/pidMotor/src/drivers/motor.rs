//! TB6612 电机控制：PWM 与两个推挽方向引脚；STBY 需由外部保持高电平。
use stm32f4xx_hal::{
    gpio::{Output, Pin, PushPull},
    timer::{CPin, PwmChannel, PwmExt, PwmHzManager},
};

/// TIMn 和 C 保证 manager 与通道属于同一定时器；P/N 表示 GPIO 端口和引脚编号。
/// manager 与通道应由板级初始化配套提供，构造函数不改变硬件状态。
pub struct Motor<TIMn, const C: u8, const P1: char, const N1: u8, const P2: char, const N2: u8>
where
    TIMn: CPin<C> + PwmExt,
{
    mgr: PwmHzManager<TIMn>,
    ch: PwmChannel<TIMn, C>,
    /// 初始化时从 HAL 获取的占空比上限；PWM 频率固定时保持有效。
    max_duty: u16,
    ain1: Pin<P1, N1, Output<PushPull>>,
    ain2: Pin<P2, N2, Output<PushPull>>,
}

impl<TIMn, const C: u8, const P1: char, const N1: u8, const P2: char, const N2: u8>
    Motor<TIMn, C, P1, N1, P2, N2>
where
    TIMn: CPin<C> + PwmExt,
{
    pub fn new(
        mgr: PwmHzManager<TIMn>,
        ch: PwmChannel<TIMn, C>,
        ain1: Pin<P1, N1, Output<PushPull>>,
        ain2: Pin<P2, N2, Output<PushPull>>,
    ) -> Self {
        let max_duty = match ch.get_max_duty() {
            // HAL 用 0 表示 ARR + 1 = 65536，u16 占空比最高只能写 65535。
            0 => u16::MAX,
            max => max,
        };
        Self {
            mgr,
            ch,
            max_duty,
            ain1,
            ain2,
        }
    }

    /// 设置带方向的 PWM 计数值（不是百分比），并启用 PWM 通道。
    /// 正数：AIN1 高、AIN2 低；负数：AIN1 低、AIN2 高；零：短路刹车。
    /// 实际机械转向取决于电机接线；绝对值超出范围时自动限幅。
    pub fn set_pwm(&mut self, pwm: i32) {
        if pwm == 0 {
            self.brake();
            return;
        }

        // unsigned_abs 避免 i32::MIN 取绝对值时溢出。
        let duty = pwm.unsigned_abs().min(u32::from(self.max_duty)) as u16;
        // 先拉高目标方向引脚，换向时经过双高刹车状态。
        if pwm > 0 {
            self.ain1.set_high();
            self.ain2.set_low();
        } else {
            self.ain2.set_high();
            self.ain1.set_low();
        }
        self.ch.set_duty(duty);
        self.ch.enable();
    }

    /// 双高使 TB6612 输出短路刹车，与自由滑行不同。
    pub fn brake(&mut self) {
        self.ain1.set_high();
        self.ain2.set_high();
        self.ch.set_duty(0);
        self.ch.enable();
    }

    /// 返回初始化时缓存的最大可写占空比计数值。
    pub fn max_duty(&self) -> u16 {
        self.max_duty
    }

    /// 仅修改占空比计数值，保持方向和通道使能状态。
    pub fn set_duty(&mut self, duty: u16) {
        self.ch.set_duty(duty.min(self.max_duty));
    }

    pub fn enable(&mut self) {
        self.ch.enable();
    }

    /// 关闭定时器通道；需要确定的 TB6612 刹车状态时请用 brake()。
    pub fn disable(&mut self) {
        self.ch.disable();
    }
}
