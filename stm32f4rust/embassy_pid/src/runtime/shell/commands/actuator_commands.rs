use super::{Command, CommandError, Reply, Request};
use crate::{
    drivers::liner_actuator::MotionError,
    runtime::tasks::actuator_task::{self, ActuatorCommand, Outcome},
};

pub(super) const ACTUATOR: Command = Command {
    name: "actuator",
    help: "actuator status|stop|home (requires retract limit)|extend/retract <ms>|extend-mm/retract-mm/goto/set-pos <mm>",
    can_opcode: None,
    run: cmd_actuator,
};

fn cmd_actuator(request: Request<'_>) -> Reply {
    let Request::Uart(args) = request else {
        return Reply::Error(CommandError::InvalidArguments);
    };
    let mut args = args.split_ascii_whitespace();
    let (action, value) = (args.next(), args.next());
    if args.next().is_some() {
        return Reply::Error(CommandError::InvalidArguments);
    }

    let command = match (action, value) {
        (None | Some("status"), None) => return status_reply(),
        (Some("stop"), None) => {
            actuator_task::request_stop();
            return Reply::text("actuator stop requested");
        }
        (Some("home"), None) => Some(ActuatorCommand::Home),
        (Some("extend"), Some(ms)) => ms.parse().ok().map(ActuatorCommand::ExtendFor),
        (Some("retract"), Some(ms)) => ms.parse().ok().map(ActuatorCommand::RetractFor),
        (Some("extend-mm"), Some(mm)) => parse_mm(mm).map(ActuatorCommand::ExtendMm),
        (Some("retract-mm"), Some(mm)) => parse_mm(mm).map(ActuatorCommand::RetractMm),
        (Some("goto"), Some(mm)) => parse_mm(mm).map(ActuatorCommand::MoveTo),
        (Some("set-pos"), Some(mm)) => parse_mm(mm).map(ActuatorCommand::SetPosition),
        _ => None,
    };

    let Some(command) = command else {
        return Reply::Error(CommandError::InvalidArguments);
    };
    if actuator_task::submit(command) {
        Reply::text("actuator command accepted")
    } else {
        Reply::Error(CommandError::Unavailable("actuator busy"))
    }
}

fn parse_mm(text: &str) -> Option<f32> {
    text.parse::<f32>().ok().filter(|mm| mm.is_finite())
}

fn status_reply() -> Reply {
    let status = actuator_task::status();
    if status.busy {
        return Reply::text("state=busy");
    }

    let result = match status.outcome {
        None => "none",
        Some(Outcome::Done) => "done",
        Some(Outcome::Cancelled) => "cancelled",
        Some(Outcome::Error(MotionError::InvalidDistance)) => "invalid_distance",
        Some(Outcome::Error(MotionError::PositionOutOfRange)) => "position_out_of_range",
        Some(Outcome::Error(MotionError::PositionUnknown)) => "position_unknown",
    };
    match status.position_mm {
        Some(mm) => Reply::formatted(format_args!(
            "state=idle position_mm={mm:.2} result={result}"
        )),
        None => Reply::formatted(format_args!(
            "state=idle position_mm=unknown result={result}"
        )),
    }
}
