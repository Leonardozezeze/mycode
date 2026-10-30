use core::ptr;

pub unsafe fn read_register(addr:*mut u32)->u32 {
    unsafe {
    ptr::read_volatile(addr)
    }
}
pub unsafe fn write_register(addr:*mut u32,value:u32) {
    unsafe {
    ptr::write_volatile(addr,value)
    }
}
pub unsafe fn reg_write_mask(reg_addr: *mut u32, value: u32, mask: u32) {
    unsafe {
        let mut v = read_register(reg_addr);
        v = (v & !mask) | (value & mask);  // 先清零 mask 区域，再写入
        write_register(reg_addr, v);
    }
}
pub unsafe fn reg_set_bits(reg_addr:*mut u32,bit_position:u8){
    assert!(bit_position<32,"bit position must less than 32!");
    let mask=0x1u32<<bit_position;
    unsafe {
        let mut value=read_register(reg_addr);
        value|=mask;
        write_register(reg_addr, value);
    }
}
pub unsafe fn reg_unset_bits(reg_addr:*mut u32,bit_position:u8){
    assert!(bit_position<32,"bit position must less than 32!");
    let mask=0x1u32<<bit_position;
    unsafe {
        let mut value=read_register(reg_addr);
        value&=!mask;
        write_register(reg_addr, value);
    }
}
pub unsafe fn reg_or_mask(reg_addr:*mut u32,mask:u32){
    unsafe {
        let mut value=read_register(reg_addr);
        value|=mask;
        write_register(reg_addr, value);
    }
}
pub unsafe fn reg_and_not_mask(reg_addr:*mut u32,mask:u32){
    unsafe {
        let mut value=read_register(reg_addr);
        value&=!mask;
        write_register(reg_addr, value);
    }
}

