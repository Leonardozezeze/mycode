//! 应用层：[`App`] 持有全部运行期状态，负责初始化和主循环调度。
//! 只做业务决策，不直接碰寄存器；分层契约见 ARCHITECTURE.md。

use crate::control::pid::Pid;
use crate::drivers::dwt::DWT;
use crate::runtime::board::{self, Devices};
use crate::runtime::config;
use crate::runtime::sched::PeriodicTask;
use core::fmt::Write; 

pub struct App {
    devices: Devices,
    pid: Pid,
    heartbeat_task: PeriodicTask,
}

impl App {
    /// 接收 board 组装好的设备，初始化 PID 和任务等业务状态。
    pub fn init() -> Self {
        #[allow(unused_mut)]
        let mut devices = board::init();
        let pid = Pid::new(
            config::PID_KP,
            config::PID_KI,
            config::PID_KD,
            -config::PID_OUTPUT_MAX,
            config::PID_OUTPUT_MAX,
        );
        let heartbeat_task = PeriodicTask::new(config::HEARTBEAT_PERIOD_MS);
        Self {
            devices,
            pid,
            heartbeat_task,
        }
    }
    /// 主循环：PeriodicTask 到点执行对应任务。
    pub fn run(mut self) -> ! {
        loop {
            let now = DWT::uptime_ms();
            if self.heartbeat_task.poll(now).is_some() {
                self.devices.green_led.toggle();
                let _=write!(self.devices.uart1,"tick {}ms\r\n",now);
            }
        }
    }
}