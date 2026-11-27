//! TB6612 单路直流电机驱动。STBY 由外部保持高电平。

use embassy_stm32::{
    Peri,
    gpio::Output,
    time::Hertz,
    timer::{
        Ch1, Channel, GeneralInstance4Channel,
        low_level::{CountingMode, OutputCompareMode, Timer},
        simple_pwm::PwmPin,
    },
};

const PWM_ARR: u16 = 1_000;
const PWM_FULL_DUTY: u16 = PWM_ARR + 1;
const PWM_CHANNEL: Channel = Channel::Ch1;
/// 归一化 PWM 指令的幅值上限，对应满占空比。
pub(crate) const PWM_COMMAND_MAX: i32 = 1_000;

pub struct Tb6612<'d, T: GeneralInstance4Channel<Word = u16>> {
    timer: Timer<'d, T>,
    _pwma: PwmPin<'d, T, Ch1>,
    ain1: Output<'d>,
    ain2: Output<'d>,
}

impl<'d, T: GeneralInstance4Channel<Word = u16>> Tb6612<'d, T> {
    /// AIN1 和 AIN2 须初始化为低电平。构造完成后电机输出为高阻态。
    pub fn new(
        timer: Peri<'d, T>,
        pwma: PwmPin<'d, T, Ch1>,
        ain1: Output<'d>,
        ain2: Output<'d>,
        tick_freq: Hertz,
    ) -> Self {
        let mut timer = Timer::new(timer);
        timer.set_counting_mode(CountingMode::EdgeAlignedUp);
        timer.set_tick_freq(tick_freq);
        timer.set_max_compare_value(PWM_ARR);
        timer.set_output_compare_mode(PWM_CHANNEL, OutputCompareMode::PwmMode1);
        timer.set_output_compare_preload(PWM_CHANNEL, true);
        // 双低方向输入配合 PWMA 高电平，使电机输出为高阻态。
        timer.set_compare_value(PWM_CHANNEL, PWM_FULL_DUTY);
        timer.generate_update_event();
        timer.enable_outputs();
        timer.enable_channel(PWM_CHANNEL, true);
        timer.start();

        Self {
            timer,
            _pwma: pwma,
            ain1,
            ain2,
        }
    }

    /// 正数驱动 AIN1 方向，负数驱动 AIN2 方向，零为短路刹车。
    /// 指令限幅到 -1000..=1000，绝对值 1000 表示满占空比。
    pub fn set_pwm(&mut self, pwm: i32) {
        if pwm == 0 {
            self.brake();
            return;
        }

        // 向上计数包含 0 和 ARR，CCR = ARR + 1 才是 100% 占空比。
        let max_command = PWM_COMMAND_MAX as u32;
        let magnitude = pwm.unsigned_abs().min(max_command);
        let duty = (magnitude * u32::from(PWM_FULL_DUTY) / max_command) as u16;

        let (active, inactive) = if pwm > 0 {
            (&mut self.ain1, &mut self.ain2)
        } else {
            (&mut self.ain2, &mut self.ain1)
        };
        if active.is_set_low() || inactive.is_set_high() {
            // 立即拉低 PWMA，避免切换方向时沿用旧占空比。
            self.timer.set_compare_value(PWM_CHANNEL, 0);
            self.timer.generate_update_event();
            active.set_high();
            inactive.set_low();
        }
        // 新占空比从下一个周期开始生效。
        self.timer.set_compare_value(PWM_CHANNEL, duty);
    }

    /// 让 TB6612 短接电机两端以刹车。
    pub fn brake(&mut self) {
        self.timer.set_compare_value(PWM_CHANNEL, 0);
        self.timer.generate_update_event();
        self.ain1.set_high();
        self.ain2.set_high();
    }

    /// 让电机自由滑行，TB6612 输出为高阻态。
    pub fn coast(&mut self) {
        self.timer.set_compare_value(PWM_CHANNEL, 0);
        self.timer.generate_update_event();
        self.ain1.set_low();
        self.ain2.set_low();
        self.timer.set_compare_value(PWM_CHANNEL, PWM_FULL_DUTY);
        self.timer.generate_update_event();
    }
}
