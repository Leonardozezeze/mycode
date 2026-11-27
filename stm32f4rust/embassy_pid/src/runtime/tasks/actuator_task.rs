//! 独占推杆驱动，处理动作请求和中途停止。

use crate::drivers::liner_actuator::{LinearActuator, MotionError};
use core::cell::Cell;
use embassy_futures::select::{Either, select};
use embassy_sync::{
    blocking_mutex::{Mutex, raw::CriticalSectionRawMutex},
    signal::Signal,
};

pub(in crate::runtime) enum ActuatorCommand {
    Home,
    ExtendFor(u32),
    RetractFor(u32),
    ExtendMm(f32),
    RetractMm(f32),
    MoveTo(f32),
    SetPosition(f32),
}

enum Request {
    Execute(ActuatorCommand),
    Stop,
}

#[derive(Clone, Copy)]
pub(in crate::runtime) enum Outcome {
    Done,
    Cancelled,
    Error(MotionError),
}

#[derive(Clone, Copy)]
pub(in crate::runtime) struct ActuatorStatus {
    pub busy: bool,
    pub position_mm: Option<f32>,
    pub outcome: Option<Outcome>,
}

static REQUEST: Signal<CriticalSectionRawMutex, Request> = Signal::new();
static STATUS: Mutex<CriticalSectionRawMutex, Cell<ActuatorStatus>> =
    Mutex::new(Cell::new(ActuatorStatus {
        busy: false,
        position_mm: None,
        outcome: None,
    }));

pub(in crate::runtime) fn submit(command: ActuatorCommand) -> bool {
    STATUS.lock(|cell| {
        let mut status = cell.get();
        if status.busy {
            return false;
        }
        status.busy = true;
        status.outcome = None;
        cell.set(status);
        REQUEST.signal(Request::Execute(command));
        true
    })
}

pub(in crate::runtime) fn request_stop() {
    STATUS.lock(|cell| {
        if cell.get().busy {
            // Stop 可覆盖尚未执行的动作，或者唤醒任务取消正在等待的动作。
            REQUEST.signal(Request::Stop);
        }
    });
}

pub(in crate::runtime) fn status() -> ActuatorStatus {
    STATUS.lock(Cell::get)
}

#[embassy_executor::task]
pub async fn actuator_task(mut actuator: LinearActuator) {
    loop {
        let outcome = match REQUEST.wait().await {
            Request::Stop => Outcome::Cancelled,
            Request::Execute(command) => {
                // 忙碌期间只接收 Stop。取消动作时驱动会通过 Drop 释放 K1/K2。
                match select(REQUEST.wait(), execute(&mut actuator, command)).await {
                    Either::First(_) => Outcome::Cancelled,
                    Either::Second(Ok(())) => Outcome::Done,
                    Either::Second(Err(error)) => Outcome::Error(error),
                }
            }
        };

        STATUS.lock(|cell| {
            REQUEST.reset();
            cell.set(ActuatorStatus {
                busy: false,
                position_mm: actuator.estimated_position_mm(),
                outcome: Some(outcome),
            });
        });
    }
}

async fn execute(
    actuator: &mut LinearActuator,
    command: ActuatorCommand,
) -> Result<(), MotionError> {
    match command {
        ActuatorCommand::Home => {
            actuator.home().await;
            Ok(())
        }
        ActuatorCommand::ExtendFor(ms) => actuator.extend_for(ms).await,
        ActuatorCommand::RetractFor(ms) => actuator.retract_for(ms).await,
        ActuatorCommand::ExtendMm(mm) => actuator.extend_mm(mm).await,
        ActuatorCommand::RetractMm(mm) => actuator.retract_mm(mm).await,
        ActuatorCommand::MoveTo(mm) => actuator.move_to(mm).await,
        ActuatorCommand::SetPosition(mm) => actuator.set_position_mm(mm),
    }
}
