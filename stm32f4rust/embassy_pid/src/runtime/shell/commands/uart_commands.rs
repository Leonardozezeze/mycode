use super::{Command, CommandError, Reply, Request};
use crate::drivers::uart::UartLink;
use embassy_sync::once_lock::OnceLock;

static UART1: OnceLock<&'static UartLink> = OnceLock::new();

pub(in crate::runtime) fn bind_uart1(uart: &'static UartLink) {
    assert!(UART1.init(uart).is_ok(), "shell UART1 already bound");
}

pub(super) const UART_STATS: Command = Command {
    name: "uart-stats",
    help: "uart-stats 显示 USART1 错误计数，summary 看摘要，errors 看接收错误明细",
    can_opcode: Some(0x02),
    run: cmd_uart_stats,
};

fn cmd_uart_stats(request: Request<'_>) -> Reply {
    let Some(uart) = UART1.try_get() else {
        return Reply::Error(CommandError::Unavailable("UART1 unavailable"));
    };

    let stats = uart.stats();
    match request {
        Request::Uart(args) => match args.trim() {
            "summary" => Reply::formatted(format_args!(
                "rx_error_events={} last_rx_error={} tx_errors={} tx_queue_full={}",
                stats.rx_error_events,
                stats.last_rx_error.unwrap_or("none"),
                stats.tx_errors,
                stats.tx_queue_full,
            )),
            "errors" => Reply::formatted(format_args!(
                "overrun={} framing={} parity={} noise={} other={}",
                stats.rx_overrun, stats.rx_framing, stats.rx_parity, stats.rx_noise, stats.rx_other,
            )),
            _ => Reply::formatted(format_args!(
                "overrun={} framing={} tx_errors={}",
                stats.rx_overrun, stats.rx_framing, stats.tx_errors,
            )),
        },
        Request::Can { opcode, args } => {
            let count = match args {
                [0x00] => stats.rx_overrun,
                [0x01] => stats.rx_framing,
                [0x02] => stats.tx_errors,
                _ => return Reply::Error(CommandError::InvalidArguments),
            };
            Reply::can(opcode, &count.to_le_bytes())
        }
    }
}
