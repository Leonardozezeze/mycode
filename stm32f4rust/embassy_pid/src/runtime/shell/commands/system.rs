use super::{Command, CommandError, Reply, Request};

pub(super) const PING: Command = Command {
    name: "ping",
    help: "检查设备是否在线",
    can_opcode: Some(0x01),
    run: cmd_ping,
};
pub(super) const ECHO: Command = Command {
    name: "echo",
    help: "打印字符",
    can_opcode: Some(0x03),
    run: cmd_echo,
};
pub(super) const SHELL_INFO: Command = Command {
    name: "shell-info",
    help: "显示 Shell 配置、命令帮助和 CAN 操作码",
    can_opcode: Some(0x05),
    run: cmd_shell_info,
};

fn cmd_ping(request: Request<'_>) -> Reply {
    match request {
        Request::Uart(_) => Reply::text("pong"),
        Request::Can { opcode, args } if args.is_empty() => Reply::can(opcode, &[]),
        Request::Can { .. } => Reply::Error(CommandError::InvalidArguments),
    }
}

fn cmd_echo(request: Request<'_>) -> Reply {
    match request {
        Request::Uart(args) => Reply::text(args),
        Request::Can { opcode, args } => Reply::can(opcode, args),
    }
}

fn cmd_shell_info(request: Request<'_>) -> Reply {
    match request {
        Request::Uart("") => Reply::ShellInfo,
        Request::Can {
            args: [low, high], ..
        } => Reply::CanShellInfoPage(u16::from_le_bytes([*low, *high])),
        _ => Reply::Error(CommandError::InvalidArguments),
    }
}
