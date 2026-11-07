use tinyrand::{Rand, Seeded, Wyrand};
use core::cell::RefCell;
use cortex_m::interrupt::Mutex;

unsafe extern "C" {
    fn Get_ADC_Seed64() -> u64;
}

static RNG: Mutex<RefCell<Option<Wyrand>>> = Mutex::new(RefCell::new(None));

/// 在系统初始化时调用一次
pub fn rng_init() {
    let mut seed = unsafe { Get_ADC_Seed64() };
    if seed == 0 {
        seed = 0x9E37_79B9_7F4A_7C15;
    }
    cortex_m::interrupt::free(|cs| {
        *RNG.borrow(cs).borrow_mut() = Some(Wyrand::seed(seed));
    });
}

/// 之后随时调用，拿下一个随机数
fn next_rand() -> u64 {
    cortex_m::interrupt::free(|cs| {
        RNG.borrow(cs)
            .borrow_mut()
            .as_mut()
            .expect("RNG not initialized")
            .next_u64()
    })
}
    
/// 返回 [0.0, 1.0) 的 f32
fn rand_unit() -> f32 {
    // 取高 24 位，映射到 [0, 1)
    (next_rand() >> 40) as f32 / (1u64 << 24) as f32
}

/// 返回 [min, max) 的 f32
fn rand_range(min: f32, max: f32) -> f32 {
    min + rand_unit() * (max - min)
}

/// 返回 [-1.0, 1.0)
fn rand_pm1() -> f32 {
    rand_unit() * 2.0 - 1.0
}

pub struct VirtualSensor {
    pub value: f32,
    pub min: f32,
    pub max: f32,
    pub step: f32,       // 单次最大相对变化量（比例，0.007 = 0.7%）
    pub spike_prob: u32, // 千分比，例如 30 表示 3% 概率大扰动
}

impl VirtualSensor {
    pub const fn new(min: f32, max: f32, init: f32, step: f32, spike_prob: u32) -> Self {
        Self { value: init, min, max, step, spike_prob }
    }

    pub fn update(&mut self) {
        let mut delta = rand_pm1() * self.step * self.value;

        // 小概率大扰动，模拟工况突变
        if (next_rand() % 1000) < self.spike_prob as u64 {
            delta += rand_pm1() * self.step * self.value * 3.0;
        }

        self.value += delta;

        // 软边界：越界后反弹一点，而不是硬截断
        if self.value < self.min {
            self.value = self.min + (self.min - self.value) * 0.3;
        }
        if self.value > self.max {
            self.value = self.max - (self.value - self.max) * 0.3;
        }
        // 硬保护
        self.value = self.value.clamp(self.min, self.max);
    }
}

pub struct SensorTask {
    last_ms: u32,
    breakage: VirtualSensor,
    impurity: VirtualSensor,
}

impl SensorTask {
    pub const fn new() -> Self {
        Self {
            last_ms: 0,
            breakage: VirtualSensor::new(1.5, 5.0, 3.0, 0.07, 10),
            impurity: VirtualSensor::new(0.8, 3.5, 2.0, 0.07, 10),
        }
    }

    /// 到 200ms 就更新并返回新值，否则返回 None
    pub fn poll(&mut self, now_ms: u32) -> Option<(f32, f32)> {
        const PERIOD_MS: u32 = 200;
        if now_ms.wrapping_sub(self.last_ms) < PERIOD_MS {
            return None;
        }
        self.last_ms = now_ms;
        self.breakage.update();
        self.impurity.update();
        Some((self.breakage.value, self.impurity.value))
    }
}