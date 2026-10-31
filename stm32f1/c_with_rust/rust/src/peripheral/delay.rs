pub struct Delay;

unsafe extern "C" {
    fn delay_us(us: u32);
    fn delay_ms(ms: u32);
}

impl Delay {
    pub fn us(us: u32) {
        unsafe { delay_us(us) }
    }

    pub fn ms(ms: u32) {
        unsafe { delay_ms(ms) }
    }
}
