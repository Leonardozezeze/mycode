//! 启动时采集 ADC 低位噪声，为虚拟传感器提供伪随机数种子。
//! PA0 应保持悬空；采样及混合不保证 64 位熵，也不用于密码学随机数。

use embassy_stm32::{
    Peri,
    adc::{Adc, SampleTime},
    gpio::Flex,
    peripherals::{ADC1, PA0},
};

/// 消耗 ADC1 和 PA0 句柄，采样完成后关闭 ADC；只在启动阶段调用一次。
pub async fn get_adc_seed64(mut adc: Adc<'_, ADC1>, mut pin: Peri<'_, PA0>) -> u64 {
    {
        let mut analog_pin = Flex::new(pin.reborrow());
        analog_pin.set_as_analog();
    }

    // 丢弃切换到模拟模式后的前几次采样。
    for _ in 0..16 {
        let _ = adc.read(&mut pin, SampleTime::CYCLES55_5).await;
    }

    // 固定采样次数，不会因低位一直不变而无限等待。
    // FNV-1a 混合每次采样的最低位；混合只能扩散已有变化，不能创造熵。
    let mut seed = 0xcbf2_9ce4_8422_2325_u64;
    for _ in 0..1024 {
        let sample = adc.read(&mut pin, SampleTime::CYCLES55_5).await;
        seed ^= u64::from(sample & 1);
        seed = seed.wrapping_mul(0x0000_0100_0000_01b3);
    }
    seed
}
