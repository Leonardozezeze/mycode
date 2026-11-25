//! 使用 Embassy 调度延时控制低电平有效的 K1/K2。
//! 位移按标称速度估算；未接位置传感器，到期释放也受任务调度影响。

use embassy_stm32::gpio::OutputOpenDrain;
use embassy_time::Timer;

/// 暂按伸出、缩回均为此速度换算，实测后可调整。
pub const SPEED_MM_PER_SECOND: f32 = 24.0;
pub const STROKE_MM: f32 = 30.0;
/// 30 mm / 24 mm/s 约 1250 ms，回零额外留出 750 ms。
/// 仅适用于到端点会自行停止的内置限位推杆，实测后可调整。
pub const HOMING_DURATION_MS: u64 = 2000;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MotionError {
    /// 距离必须是有限的非负数，单次位移不能超过总行程。
    InvalidDistance,
    /// 当前或目标位置必须在 0～STROKE_MM 之间。
    PositionOutOfRange,
    /// 上电或动作被取消后，需重新登记确认过的位置才能绝对定位。
    PositionUnknown,
}

pub struct LinearActuator {
    k1: OutputOpenDrain<'static>,
    k2: OutputOpenDrain<'static>,
    position_mm: Option<f32>,
}

impl LinearActuator {
    pub fn new(mut k1: OutputOpenDrain<'static>, mut k2: OutputOpenDrain<'static>) -> Self {
        // 开漏的 High 表示释放引脚，由外部电路恢复无效电平。
        k1.set_high();
        k2.set_high();
        Self {
            k1,
            k2,
            position_mm: None,
        }
    }

    /// 持续缩回至内置限位，再登记估算零点；等待期间其他任务可运行。
    /// 必须有内置缩回限位保护。无到位反馈，计时结束不会验证是否真的到位。
    /// 取消回零时释放输出并保留未知位置。
    pub async fn home(&mut self) {
        self.stop();
        self.position_mm = None;
        self.k2.set_low();
        let motion = ActiveMotion(self);
        Timer::after_millis(HOMING_DURATION_MS).await;
        motion.0.position_mm = Some(0.0);
    }

    /// 伸出指定毫秒；等待期间其他任务可以运行，零时长不动作。
    pub async fn extend_for(&mut self, duration_ms: u32) -> Result<(), MotionError> {
        let distance = SPEED_MM_PER_SECOND * duration_ms as f32 / 1000.0;
        self.move_by(distance).await
    }

    /// 缩回指定毫秒；等待期间其他任务可以运行，零时长不动作。
    pub async fn retract_for(&mut self, duration_ms: u32) -> Result<(), MotionError> {
        let distance = SPEED_MM_PER_SECOND * duration_ms as f32 / 1000.0;
        self.move_by(-distance).await
    }

    /// 相对伸出指定毫米，按标称速度换算时间，取最近的毫秒，非零位移至少 1 ms。
    pub async fn extend_mm(&mut self, distance_mm: f32) -> Result<(), MotionError> {
        if distance_mm < 0.0 {
            return Err(MotionError::InvalidDistance);
        }
        self.move_by(distance_mm).await
    }

    /// 相对缩回指定毫米，按标称速度换算时间，取最近的毫秒，非零位移至少 1 ms。
    pub async fn retract_mm(&mut self, distance_mm: f32) -> Result<(), MotionError> {
        if distance_mm < 0.0 {
            return Err(MotionError::InvalidDistance);
        }
        self.move_by(-distance_mm).await
    }

    /// 移到估算的绝对位置，0 表示完全缩回，30 表示完全伸出。
    /// 必须先登记已知位置；这不是位置传感器闭环控制。
    pub async fn move_to(&mut self, target_mm: f32) -> Result<(), MotionError> {
        check_position(target_mm)?;
        let current = self.position_mm.ok_or(MotionError::PositionUnknown)?;
        self.move_by(target_mm - current).await
    }

    /// 登记实际确认过的当前位置，不驱动推杆，也不执行自动回零。
    pub fn set_position_mm(&mut self, position_mm: f32) -> Result<(), MotionError> {
        check_position(position_mm)?;
        self.position_mm = Some(position_mm);
        Ok(())
    }

    /// 返回估算位置；尚未标定或动作被取消时为 None。
    pub fn estimated_position_mm(&self) -> Option<f32> {
        self.position_mm
    }

    pub fn stop(&mut self) {
        self.k1.set_high();
        self.k2.set_high();
    }

    async fn move_by(&mut self, delta_mm: f32) -> Result<(), MotionError> {
        let duration_ms = duration_for_distance(delta_mm.abs())?;
        if delta_mm == 0.0 {
            return Ok(());
        }
        let target = match self.position_mm {
            Some(current) => {
                let target = current + delta_mm;
                check_position(target)?;
                Some(target)
            }
            None => None,
        };
        self.stop();
        // 部分动作被取消时无法确认实际位移，只有完整结束才恢复估算位置。
        self.position_mm = None;
        if delta_mm > 0.0 {
            self.k1.set_low();
        } else {
            self.k2.set_low();
        }
        let motion = ActiveMotion(self);
        Timer::after_millis(u64::from(duration_ms)).await;
        motion.0.position_mm = target;
        Ok(())
    }
}

impl Drop for LinearActuator {
    fn drop(&mut self) {
        self.stop();
    }
}

/// 在正常结束或 future 被丢弃时释放两个输出。
struct ActiveMotion<'a>(&'a mut LinearActuator);
impl Drop for ActiveMotion<'_> {
    fn drop(&mut self) {
        self.0.stop();
    }
}

fn check_position(position_mm: f32) -> Result<(), MotionError> {
    if !position_mm.is_finite() || !(0.0..=STROKE_MM).contains(&position_mm) {
        return Err(MotionError::PositionOutOfRange);
    }
    Ok(())
}

pub(crate) fn duration_for_distance(distance_mm: f32) -> Result<u32, MotionError> {
    if !distance_mm.is_finite() || !(0.0..=STROKE_MM).contains(&distance_mm) {
        return Err(MotionError::InvalidDistance);
    }
    let millis = distance_mm / SPEED_MM_PER_SECOND * 1000.0;
    if distance_mm == 0.0 {
        return Ok(0);
    }
    // 取最近毫秒，避免时间接口经浮点位移换算后被向上取整多出 1 ms。
    Ok(((millis + 0.5) as u32).max(1))
}
