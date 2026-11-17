//! 调试串口接收任务：记录收到的原始数据及错误，供调试器查看。

use crate::comm::debug::DebugRxState;
use crate::drivers::uart::{RxEvent, UartLink};
use crate::runtime::config;
use embassy_stm32::usart::Error;

/// 调试串口的接收状态，仅由本任务修改。
struct DebugUartState {
    rx: DebugRxState<{ config::UART_RX_CHUNK_BYTES }>,
    last_rx_error: Option<Error>,
}

impl DebugUartState {
    fn new() -> Self {
        Self {
            rx: DebugRxState::new(),
            last_rx_error: None,
        }
    }
}

/// 同一调试串口只由一个任务消费接收事件。
#[embassy_executor::task]
pub(crate) async fn debug_uart_task(uart: &'static UartLink) {
    let mut state = DebugUartState::new();
    loop {
        match uart.receive().await {
            RxEvent::Data(data) => state.rx.on_data(&data),
            RxEvent::Error(error) => {
                state.last_rx_error = Some(error);
                state.rx.on_error();
            }
        }
    }
}
