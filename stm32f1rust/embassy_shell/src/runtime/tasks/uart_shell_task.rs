//! UART Shell 任务：将串口接收事件交给 Shell。

use crate::drivers::uart::{RxEvent, UartLink};
use crate::runtime::shell::{config::UART_MODE, shell::Shell};

/// 同一调试串口只由一个任务消费接收事件。
#[embassy_executor::task]
pub(crate) async fn uart_shell_task(uart: &'static UartLink) {
    let mut shell = Shell::new(uart, UART_MODE);
    shell.show_prompt().await;
    loop {
        match uart.receive().await {
            RxEvent::Data(data) => shell.feed(&data).await,
            RxEvent::Error(_) => shell.discard_line(),
        }
    }
}
