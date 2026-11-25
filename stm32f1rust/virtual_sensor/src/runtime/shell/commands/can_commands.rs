use super::{Command, CommandError, Reply, Request};
use crate::drivers::can::CanLink;
use embassy_sync::once_lock::OnceLock;

static CAN1: OnceLock<&'static CanLink> = OnceLock::new();

pub(in crate::runtime) fn bind_can1(can: &'static CanLink) {
    assert!(CAN1.init(can).is_ok(), "shell CAN1 already bound");
}

pub(super) const CAN_STATS: Command = Command {
    name: "can-stats",
    help: "can-stats 显示 CAN1 摘要，can-stats errors 显示分类错误计数",
    can_opcode: Some(0x04),
    run: cmd_can_stats,
};

fn cmd_can_stats(request: Request<'_>) -> Reply {
    let Some(can) = CAN1.try_get() else {
        return Reply::Error(CommandError::Unavailable("CAN1 unavailable"));
    };

    let stats = can.stats();
    match request {
        Request::Uart(args) if args.trim() == "errors" => {
            let errors = stats.bus_errors;
            Reply::formatted(format_args!(
                "stuff={} form={} ack={} recessive={} dominant={} crc={} software={}",
                errors.stuff,
                errors.form,
                errors.ack,
                errors.bit_recessive,
                errors.bit_dominant,
                errors.crc,
                errors.software,
            ))
        }
        Request::Uart(_) => Reply::formatted(format_args!(
            "bus_error_events={} last_bus_error={} tx_queue_full={}",
            stats.bus_error_events,
            stats.last_bus_error.unwrap_or("none"),
            stats.tx_queue_full,
        )),
        Request::Can { opcode, args } => {
            let count = match args {
                [0x00] => stats.bus_error_events,
                [0x01] => stats.tx_queue_full,
                _ => return Reply::Error(CommandError::InvalidArguments),
            };
            Reply::can(opcode, &count.to_le_bytes())
        }
    }
}
