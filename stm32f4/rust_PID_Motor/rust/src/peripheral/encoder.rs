//! 编码器（TIM4，PD12/PD13，TI12 四倍频）
//!
//! C 侧 `ffi_app/Src/encoder.c` 只做寄存器读写；
//! 增量/回绕处理放在 Rust 侧，`algo` 里做 PID 时直接用 [`Encoder::delta`]。

use crate::algo::speed::SpeedMeter;

pub struct Encoder {
    /// 上一次读取的计数，用于算增量
    last: i16,
    last_time:u64,
}

unsafe extern "C" {
    fn encoder_init();
    fn encoder_get_count() -> i16;
    fn encoder_reset();
}

impl Encoder {
    pub const fn new() -> Self {
        Self { last: 0,last_time:0}
    }

    /// 启动计数（C 侧 `board_drive_init()` 已调用，这里可用于重新启动）
    pub fn init(&self) {
        unsafe { encoder_init() }
    }

    /// 当前 16 位原始计数（有符号，范围 -32768..=32767）
    pub fn count(&self) -> i16 {
        unsafe { encoder_get_count() }
    }

    /// 距上次调用以来的计数增量，自动处理 16 位回绕。
    ///
    /// 只要两次读取之间的增量小于 32768（正常采样频率下必然成立），
    /// 结果就是准确的；返回 `i32` 避免上层再做窄化。
    pub fn delta(&mut self) -> i32 {
        let now = self.count();
        let delta = now.wrapping_sub(self.last) as i32;
        self.last = now;
        delta
    }

    /// 计数清零，并重置内部基准
    pub fn reset(&mut self) {
        unsafe { encoder_reset() };
        self.last = 0;
    }

    /// 周期采样任务：距上次采样满 `dt_ms` 就返回 `Some((增量, 实际间隔ms))`，
    /// 时间没到返回 `None`。主循环里轮询调用即可。
    ///
    /// 用实际间隔（而不是名义 `dt_ms`）算转速，可消掉主循环轮询的抖动误差。
    pub fn get_delta_task(&mut self, now_ms: u64, dt_ms: u64) -> Option<(i32, f32)> {
        let elapsed = now_ms - self.last_time; // 先算，再更新 last_time
        if elapsed < dt_ms {
            return None; // 时间没到，提前返回
        }
        self.last_time = now_ms; // 记下本次采样时刻，下次再比
        Some((self.delta(), elapsed as f32))
    }
}

pub struct EncoderSpeed{enc:Encoder,speeder:SpeedMeter}
impl EncoderSpeed{
    pub const  fn new(counts_per_rev: f32)->Self{
        Self { enc: Encoder::new(), speeder: SpeedMeter::new(counts_per_rev) }
    }
    /// 采样时刻到了就返回当前转速 RPM，没到返回 `None`。
    pub fn update_task(&mut self, now_ms: u64, dt_ms: u64) -> Option<f32> {
        let (delta, dt) = self.enc.get_delta_task(now_ms, dt_ms)?;
        Some(self.speeder.m_update(delta, dt))
    }
}