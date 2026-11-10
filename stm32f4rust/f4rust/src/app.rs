//! 应用层：[`App`] 持有全部运行期状态，负责初始化和主循环调度。
//! 只做业务决策，不直接碰寄存器；分层契约见 ARCHITECTURE.md。

use crate::algo::sched::PeriodicTask;
use crate::board::{self, Board, DeviceParts};
use crate::control::pid::Pid;
use crate::drivers::dwt::DWT;
use crate::drivers::uart::{self, UartLink};
use core::fmt::Write as _;
use stm32f4xx_hal::pac::USART1;

pub struct App {
    board: Board,
    /// USART1 器件对象（当前只有阻塞发送）
    uart1: UartLink<USART1>,
    pid: Pid,
    heartbeat_task: PeriodicTask,
}

impl App {
    /// 初始化：接盘、把零件组装成器件、构造业务状态。
    /// 这里也是资源移交给中断侧的唯一移交点。
    pub fn init() -> Self {
        let (board, parts) = board::init();
        let DeviceParts { uart1_tx, uart1_rx } = parts;
        let uart1 = uart::uart1(uart1_tx, uart1_rx);
        let pid = Pid::new(1.0, 0.1, 0.0, -100.0, 100.0);
        let heartbeat_task = PeriodicTask::new(400);
        Self {
            board,
            uart1,
            pid,
            heartbeat_task,
        }
    }

    /// 主循环：PeriodicTask 到点执行对应任务。
    pub fn run(mut self) -> ! {
        loop {
            let now = DWT::uptime_ms();
            // 心跳任务：翻灯 + 串口冒烟测试
            if self.heartbeat_task.poll(now).is_some() {
                self.board.green_led.toggle();
                writeln!(self.uart1, "t={}ms\r", now).ok();
            }
            // if now > 3000 { panic!("test panic from app"); }
        }
    }
}
