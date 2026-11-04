//! 直流电机抽象（TB6612 驱动板：TIM3 CH1 PWM + PA4/PA5 方向脚）
//!
//! C 侧 `ffi_app/Src/tb6612.c` 以芯片命名，提供芯片级原语和真值表译码；
//! 这里是板级 "Motor" 抽象——以后换驱动芯片（DRV8833 之类）只需换 C 文件，
//! 本模块 API 不变。策略类逻辑（斜坡、软启动、堵转保护）也放这里。

unsafe extern "C" {
    fn tb6612_init();
    fn tb6612_set_pwm(pwm: u16);
    /// ain1 / ain2：0 = 低电平，非 0 = 高电平
    fn tb6612_set_dir(ain1: u8, ain2: u8);
    fn tb6612_enable();
    fn tb6612_disable();
    /// 带符号调速：正 = 正转，负 = 反转，幅值 = 占空比（C 侧自动钳位）
    fn tb6612_set_speed(speed: i16);
}

/// 电机（TB6612 驱动）。无内部状态，方法组合 C 原语实现语义。
pub struct Motor;

impl Motor {
    /// 占空比上限，与 C 侧 `TB6612_PWM_MAX`（TIM3 ARR+1）保持一致
    pub const PWM_MAX: u16 = 1000;

    pub const fn new() -> Self {
        Self
    }

    /// 启动 PWM 输出并置入安全状态（`board_drive_init()` 已调用，可用于重新启动）
    pub fn init(&self) {
        unsafe { tb6612_init() }
    }

    /// 使能芯片（STBY 拉高）。STBY 直连 3.3V 时 C 侧为空操作
    pub fn enable(&self) {
        unsafe { tb6612_enable() }
    }

    /// 失能芯片（STBY 拉低），电机断电滑行
    pub fn disable(&self) {
        unsafe { tb6612_disable() }
    }

    /// 带符号调速，PID 输出直接喂这里：
    /// 正 = 正转，负 = 反转，幅值 = 占空比（超出自动钳到 `PWM_MAX`）
    pub fn set_speed(&self, speed: i16) {
        unsafe { tb6612_set_speed(speed) }
    }

    /// 正转（CW）：AIN1=1，AIN2=0
    pub fn forward(&self, pwm: u16) {
        unsafe {
            tb6612_set_dir(1, 0); // 先定方向，再上功率
            tb6612_set_pwm(pwm.min(Self::PWM_MAX));
        }
    }

    /// 反转（CCW）：AIN1=0，AIN2=1
    pub fn backward(&self, pwm: u16) {
        unsafe {
            tb6612_set_dir(0, 1);
            tb6612_set_pwm(pwm.min(Self::PWM_MAX));
        }
    }

    /// 滑行停止：先关 PWM 再断方向脚，避免瞬间反向电流，电机自由滑行
    pub fn stop(&self) {
        unsafe {
            tb6612_set_pwm(0);
            tb6612_set_dir(0, 0);
        }
    }

    /// 短路刹车：PWM=0、两方向脚全高，绕组短路快速刹停
    pub fn brake(&self) {
        unsafe {
            tb6612_set_pwm(0);
            tb6612_set_dir(1, 1);
        }
    }
}
