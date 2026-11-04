pub struct Delay;

unsafe extern "C" {
    fn delay_us(us: u32);
    fn delay_ms(ms: u32);
}
unsafe extern "C" {
    fn HAL_GetTick() -> u32;
}
impl Delay {
    pub fn us(us: u32) {
        unsafe { delay_us(us) }
    }

    pub fn ms(ms: u32) {
        unsafe { delay_ms(ms) }
    }
}
#[inline]
pub fn now_ms() -> u32 {
    unsafe { HAL_GetTick() }
}
