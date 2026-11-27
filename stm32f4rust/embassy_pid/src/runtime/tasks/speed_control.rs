//! 电机闭环调速和手动 PWM，独占编码器和电机驱动。

use crate::{
    control::{pid::Pid, speed::SpeedEstimator},
    drivers::{
        encoder::Encoder,
        tb6612::{PWM_COMMAND_MAX, Tb6612},
    },
    runtime::config::{
        pid::{PID_GAIN_MAX, PID_KD, PID_KI, PID_KP, PID_OUTPUT_MAX},
        speed::{
            COUNTS_PER_REV, SPEED_FILTER_TIME_CONSTANT_MS, SPEED_SAMPLE_PERIOD_MS,
            SPEED_TARGET_MAX_RPM,
        },
    },
};
use core::cell::Cell;
use embassy_stm32::peripherals::TIM3;
use embassy_sync::blocking_mutex::{Mutex, raw::CriticalSectionRawMutex};
use embassy_time::{Duration, Timer};

const DEFAULT_PID_GAINS: [f32; 3] = [PID_KP, PID_KI, PID_KD];

#[derive(Clone, Copy)]
enum ControlMode {
    Pid { reset: bool },
    Manual(i32),
}

#[derive(Clone, Copy)]
pub(in crate::runtime) struct SpeedStatus {
    pub target_rpm: f32,
    pub raw_rpm: f32,
    pub rpm: f32,
    pub pwm: i32,
    pub encoder_count: u16,
    pub encoder_delta: i16,
    pub sample_interval: Duration,
    pub pid_gains: [f32; 3],
    mode: ControlMode,
}

impl SpeedStatus {
    pub(in crate::runtime) fn pid_enabled(self) -> bool {
        matches!(self.mode, ControlMode::Pid { .. })
    }
}

static STATUS: Mutex<CriticalSectionRawMutex, Cell<SpeedStatus>> =
    Mutex::new(Cell::new(SpeedStatus {
        target_rpm: 0.0,
        raw_rpm: 0.0,
        rpm: 0.0,
        pwm: 0,
        encoder_count: 0,
        encoder_delta: 0,
        sample_interval: Duration::MIN,
        pid_gains: DEFAULT_PID_GAINS,
        mode: ControlMode::Pid { reset: false },
    }));

pub(in crate::runtime) fn set_target_rpm(target_rpm: f32) -> bool {
    if !target_rpm.is_finite() || target_rpm.abs() > SPEED_TARGET_MAX_RPM {
        return false;
    }

    STATUS.lock(|cell| {
        let mut status = cell.get();
        status.target_rpm = target_rpm;
        // speed 0 在手动模式下也停止驱动。
        if target_rpm == 0.0 && !status.pid_enabled() {
            status.mode = ControlMode::Manual(0);
        }
        cell.set(status);
    });
    true
}

pub(in crate::runtime) fn set_manual_pwm(pwm: i32) -> bool {
    if !(-PWM_COMMAND_MAX..=PWM_COMMAND_MAX).contains(&pwm) {
        return false;
    }

    STATUS.lock(|cell| {
        let mut status = cell.get();
        status.mode = ControlMode::Manual(pwm);
        cell.set(status);
    });
    true
}

pub(in crate::runtime) fn set_pid_enabled(enabled: bool) {
    STATUS.lock(|cell| {
        let mut status = cell.get();
        if !enabled {
            status.mode = ControlMode::Manual(0);
        } else if !status.pid_enabled() {
            status.mode = ControlMode::Pid { reset: true };
        }
        cell.set(status);
    });
}

/// 传入 None 时恢复固件默认增益。
pub(in crate::runtime) fn set_pid_gains(gains: Option<[f32; 3]>) -> bool {
    if let Some(values) = gains
        && values
            .iter()
            .any(|gain| !(0.0..=PID_GAIN_MAX).contains(gain))
    {
        return false;
    }

    STATUS.lock(|cell| {
        let mut status = cell.get();
        status.pid_gains = gains.unwrap_or(DEFAULT_PID_GAINS);
        cell.set(status);
    });
    true
}

pub(in crate::runtime) fn stop() {
    STATUS.lock(|cell| {
        let mut status = cell.get();
        status.target_rpm = 0.0;
        status.mode = ControlMode::Manual(0);
        cell.set(status);
    });
}

pub(in crate::runtime) fn status() -> SpeedStatus {
    STATUS.lock(Cell::get)
}

#[embassy_executor::task]
pub async fn speed_control_task(mut motor: Tb6612<'static, TIM3>, encoder: Encoder<'static>) {
    let period = Duration::from_millis(SPEED_SAMPLE_PERIOD_MS);
    let filter_time = core::time::Duration::from_millis(SPEED_FILTER_TIME_CONSTANT_MS);
    let mut estimator =
        SpeedEstimator::new(COUNTS_PER_REV, filter_time).expect("invalid encoder counts per rev");
    let mut pid = Pid::new(PID_KP, PID_KI, PID_KD, 0.0, PID_OUTPUT_MAX);
    let mut applied_gains = DEFAULT_PID_GAINS;
    let mut previous_direction = 0.0;
    let mut previous_sample = encoder.snapshot();

    loop {
        Timer::at(previous_sample.timestamp + period).await;
        let current = encoder.snapshot();
        let sample = current.sample_since(previous_sample);
        previous_sample = current;
        let elapsed: core::time::Duration = sample.elapsed.into();
        let estimate = estimator.update(sample.counts, elapsed);

        let current_status = STATUS.lock(|cell| {
            let current = cell.get();
            // 即使两拍之间完成了手动到 PID 的切换，也要响应重置请求。
            if let ControlMode::Pid { reset: true } = current.mode {
                cell.set(SpeedStatus {
                    mode: ControlMode::Pid { reset: false },
                    ..current
                });
            }
            current
        });

        let pwm = match current_status.mode {
            ControlMode::Manual(pwm) => pwm,
            ControlMode::Pid { reset } => {
                let target = current_status.target_rpm;
                let direction = if target == 0.0 { 0.0 } else { target.signum() };
                let gains_changed = current_status.pid_gains != applied_gains;
                if gains_changed {
                    applied_gains = current_status.pid_gains;
                    let [kp, ki, kd] = applied_gains;
                    pid.set_gains(kp, ki, kd);
                }
                if reset || gains_changed || direction != previous_direction || estimate.is_none() {
                    pid.reset();
                }
                previous_direction = direction;

                // PID 输出是目标方向上的占空比幅值，超速时可减到零。
                match &estimate {
                    Some(speed) if direction != 0.0 => {
                        let duty =
                            pid.update(target.abs(), direction * speed.rpm, elapsed.as_secs_f32());
                        (direction * duty) as i32
                    }
                    _ => 0,
                }
            }
        };
        if pwm != current_status.pwm {
            if pwm == 0 {
                motor.coast();
            } else {
                motor.set_pwm(pwm);
            }
        }

        STATUS.lock(|cell| {
            let mut status = cell.get();
            if let Some(speed) = estimate {
                status.raw_rpm = speed.raw_rpm;
                status.rpm = speed.rpm;
            }
            status.pwm = pwm;
            status.encoder_count = current.count();
            status.encoder_delta = sample.counts;
            status.sample_interval = sample.elapsed;
            cell.set(status);
        });
    }
}
