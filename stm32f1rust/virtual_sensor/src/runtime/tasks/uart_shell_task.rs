//! UART Shell 任务：将串口接收事件交给 Shell。

use crate::drivers::uart::{RxEvent, UartLink};
use crate::runtime::shell::shell::{Shell, UartMode};

/// 同一调试串口只由一个任务消费接收事件。
#[embassy_executor::task]
pub(crate) async fn uart_shell_task(uart: &'static UartLink) {
    let mut shell = Shell::new(uart, UartMode::Terminal);
    shell.show_prompt();
    loop {
        match uart.receive().await {
            RxEvent::Data(data) => shell.feed(&data),
            RxEvent::Error(_) => shell.discard_line(),
        }
    }
}
