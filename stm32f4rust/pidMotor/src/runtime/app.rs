//! 应用层：[`App`] 持有全部运行期状态，负责初始化和主循环调度。
//! 只做业务决策，不直接碰寄存器；分层契约见 ARCHITECTURE.md。

use crate::comm::proto::{SensorFrame, SensorParser, encode_telemetry};
use crate::control::pid::Pid;
use crate::drivers::dwt::DWT;
use crate::runtime::board::{self, Devices};
use crate::runtime::config;
use crate::runtime::sched::PeriodicTask;

pub struct App {
    devices: Devices,
    pid: Pid,
    heartbeat_task: PeriodicTask,
    speed_task: PeriodicTask,
    /// 输出轴平均转速，单位 RPM，供业务/PID 使用。
    motor_speed_rpm: f32,
    /// 首个有效传感器帧到达前目标为 0，随后由破碎率映射得到。
    target_rpm: f32,
    sensor_parser: SensorParser,
    /// 最近一帧完整的传感器数据；收到首帧前为 None。
    sensor_frame: Option<SensorFrame>,
}

impl App {
    /// 接收 board 组装好的设备，初始化 PID 和任务等业务状态。
    pub fn init() -> Self {
        let mut devices = board::init();

        let pid = Pid::new(
            config::PID_KP,
            config::PID_KI,
            config::PID_KD,
            -config::PID_OUTPUT_MAX,
            config::PID_OUTPUT_MAX,
        );
        let heartbeat_task = PeriodicTask::new(config::HEARTBEAT_PERIOD_MS);
        devices.motor.set_pwm(0);
        let sample_start_ms = DWT::uptime_ms();
        // 丢弃初始化期间的计数差，将首次采样基准与任务起点对齐。
        devices.encoder.delta_count();
        let speed_task = PeriodicTask::new_at(config::SPEED_SAMPLE_PERIOD_MS, sample_start_ms);
        Self {
            devices,
            pid,
            heartbeat_task,
            speed_task,
            motor_speed_rpm: 0.0,
            target_rpm: 0.0,
            sensor_parser: SensorParser::new(config::SENSOR_BYTE_ORDER),
            sensor_frame: None,
        }
    }

    /// 主循环：PeriodicTask 到点执行对应任务。
    pub fn run(mut self) -> ! {
        loop {
            let now = DWT::uptime_ms();
            // 只处理当前已有的字节，不等待下一字节；解析状态跨循环保留。
            for _ in 0..config::SENSOR_RX_BUDGET {
                let Some(byte) = self.devices.uart5.read_byte() else {
                    break;
                };
                if let Some(frame) = self.sensor_parser.feed(byte) {
                    // 非有限浮点值不能进入目标映射和 PID 状态。
                    if !frame.value1.is_finite() || !frame.value2.is_finite() {
                        continue;
                    }
                    self.sensor_frame = Some(frame);
                    self.target_rpm = breakage_rate_to_target(frame.value1);
                    let bytes = encode_telemetry(frame, self.motor_speed_rpm, self.target_rpm);
                    // 整帧非阻塞入队；队列满时跳过本次回传，控制任务继续运行。
                    self.devices.uart1.send(&bytes).ok();
                }
            }
            // 每拍测速并闭环控制；PID 使用任务返回的实际时间间隔（秒）。
            if let Some(elapsed_ms) = self.speed_task.poll(now) {
                let delta = self.devices.encoder.delta_count();
                self.motor_speed_rpm =
                    f32::from(delta) * 60_000.0 / elapsed_ms as f32 / config::COUNTS_PER_REV;
                let dt = elapsed_ms as f32 / 1_000.0;
                let output = self.pid.update(self.target_rpm, self.motor_speed_rpm, dt);
                let pwm =
                    output / config::PID_OUTPUT_MAX * f32::from(self.devices.motor.max_duty());
                self.devices.motor.set_pwm(pwm as i32);
            }
            // 心跳只翻灯。
            if self.heartbeat_task.poll(now).is_some() {
                self.devices.green_led.toggle();
            }
        }
    }
}

/// 破碎率越低，目标转速越高；越界输入按映射边界处理。
fn breakage_rate_to_target(b: f32) -> f32 {
    let b = b.clamp(config::B_MIN, config::B_MAX);
    config::RPM_MAX
        - (b - config::B_MIN) * (config::RPM_MAX - config::RPM_MIN)
            / (config::B_MAX - config::B_MIN)
}
