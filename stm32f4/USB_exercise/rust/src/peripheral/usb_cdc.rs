//! USB CDC 虚拟串口发送
//!
//! C 侧 `usb_cdc_write` 先把数据拷进静态缓冲再交给 USB 外设，
//! 所以这里可以安全地传 `&str` 的临时指针。
use core::fmt;

/// 单次交给 C 侧的最大字节数（内部缓冲 APP_TX_DATA_SIZE = 2048）
pub const MAX_WRITE: usize = 1024;

unsafe extern "C" {
    /// 成功返回 0；-1 长度超限，-2 发送忙，-3 USB 未枚举
    fn usb_cdc_write(data: *const u8, len: u16) -> i32;
}

pub struct CdcWriter;

impl fmt::Write for CdcWriter {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        let mut rest = s.as_bytes();
        while !rest.is_empty() {
            let len = rest.len().min(MAX_WRITE) as u16;
            let ok = unsafe { usb_cdc_write(rest.as_ptr(), len) == 0 };
            if !ok {
                return Err(fmt::Error);
            }
            rest = &rest[len as usize..];
        }
        Ok(())
    }
}

pub fn print(args: fmt::Arguments<'_>) {
    use fmt::Write;
    let _ = CdcWriter.write_fmt(args);
}

#[macro_export]
macro_rules! cdc_println {
    ($($arg:tt)*) => {
        $crate::peripheral::usb_cdc::print(
            format_args!("{}\r\n", format_args!($($arg)*))
        )
    };
}

#[macro_export]
macro_rules! cdc_print {
    ($($arg:tt)*) => {
        $crate::peripheral::usb_cdc::print(
            format_args!("{}", format_args!($($arg)*))
        )
    };
}
