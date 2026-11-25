//! 调试串口接收任务：记录收到的原始数据及错误，供调试器查看。

use crate::drivers::uart::{RxEvent, UartLink};


/// 同一调试串口只由一个任务消费接收事件。
#[embassy_executor::task]
pub(crate) async fn debug_uart_task(uart: &'static UartLink) {
    loop {
        match uart.receive().await {
            #[allow(unused)]
            RxEvent::Data(data) => {

            },
            #[allow(unused)]
            RxEvent::Error(error) => {

            },
        }
    }
}
