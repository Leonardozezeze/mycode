use super::{Command, CommandError, Reply, Request};
use crate::runtime::tasks::speed_control;

pub(super) const SPEED: Command = Command {
    name: "speed",
    help: "speed 查看调速状态，speed <rpm> 设 PID 目标转速，speed 0 停机滑行",
    can_opcode: None,
    run: cmd_speed,
};

pub(super) const PWM: Command = Command {
    name: "pwm",
    help: "pwm <值> 切到手动 PWM，范围 -1000..1000，0 停机滑行",
    can_opcode: None,
    run: cmd_pwm,
};

pub(super) const PID: Command = Command {
    name: "pid",
    help: "pid [on|off|gains [<kp> <ki> <kd>]|defaults]，在线增益范围 0..1000",
    can_opcode: None,
    run: cmd_pid,
};

pub(super) const ENCODER: Command = Command {
    name: "encoder",
    help: "查看最近一拍的硬件计数、净计数和实际采样间隔",
    can_opcode: None,
    run: cmd_encoder,
};

pub(super) const STOP: Command = Command {
    name: "stop",
    help: "关闭 PID、清除目标转速和手动 PWM，电机滑行",
    can_opcode: None,
    run: cmd_stop,
};

fn cmd_speed(request: Request<'_>) -> Reply {
    let Request::Uart(args) = request else {
        return Reply::Error(CommandError::InvalidArguments);
    };
    let args = args.trim();
    if !args.is_empty() {
        let Ok(target_rpm) = args.parse::<f32>() else {
            return Reply::Error(CommandError::InvalidArguments);
        };
        if !speed_control::set_target_rpm(target_rpm) {
            return Reply::Error(CommandError::InvalidArguments);
        }
    }

    let status = speed_control::status();
    Reply::formatted(format_args!(
        "target_rpm={:.2} raw_rpm={:.2} rpm={:.2} pwm={}",
        status.target_rpm, status.raw_rpm, status.rpm, status.pwm,
    ))
}

fn cmd_pwm(request: Request<'_>) -> Reply {
    let Request::Uart(args) = request else {
        return Reply::Error(CommandError::InvalidArguments);
    };
    let Ok(pwm) = args.trim().parse::<i32>() else {
        return Reply::Error(CommandError::InvalidArguments);
    };
    if !speed_control::set_manual_pwm(pwm) {
        return Reply::Error(CommandError::InvalidArguments);
    }

    Reply::formatted(format_args!("manual_pwm={pwm} pid=off"))
}

fn cmd_pid(request: Request<'_>) -> Reply {
    let Request::Uart(args) = request else {
        return Reply::Error(CommandError::InvalidArguments);
    };
    match args.trim() {
        "" => {}
        "on" => speed_control::set_pid_enabled(true),
        "off" => speed_control::set_pid_enabled(false),
        "gains" => return pid_gains_reply(),
        "defaults" => {
            speed_control::set_pid_gains(None);
            return pid_gains_reply();
        }
        args => return set_pid_gains(args),
    }

    Reply::text(if speed_control::status().pid_enabled() {
        "pid=on"
    } else {
        "pid=off"
    })
}

fn set_pid_gains(args: &str) -> Reply {
    let mut args = args.split_ascii_whitespace();
    let (Some("gains"), Some(kp), Some(ki), Some(kd), None) = (
        args.next(),
        args.next(),
        args.next(),
        args.next(),
        args.next(),
    ) else {
        return Reply::Error(CommandError::InvalidArguments);
    };
    let [Ok(kp), Ok(ki), Ok(kd)] = [kp, ki, kd].map(str::parse::<f32>) else {
        return Reply::Error(CommandError::InvalidArguments);
    };
    if !speed_control::set_pid_gains(Some([kp, ki, kd])) {
        return Reply::Error(CommandError::InvalidArguments);
    }

    pid_gains_reply()
}

fn pid_gains_reply() -> Reply {
    let [kp, ki, kd] = speed_control::status().pid_gains;
    Reply::formatted(format_args!("kp={kp:?} ki={ki:?} kd={kd:?}"))
}

fn cmd_encoder(request: Request<'_>) -> Reply {
    if !matches!(request, Request::Uart("")) {
        return Reply::Error(CommandError::InvalidArguments);
    }
    let status = speed_control::status();
    Reply::formatted(format_args!(
        "count={} delta={} dt_us={}",
        status.encoder_count,
        status.encoder_delta,
        status.sample_interval.as_micros(),
    ))
}

fn cmd_stop(request: Request<'_>) -> Reply {
    if !matches!(request, Request::Uart("")) {
        return Reply::Error(CommandError::InvalidArguments);
    }
    speed_control::stop();
    Reply::text("stop requested")
}
