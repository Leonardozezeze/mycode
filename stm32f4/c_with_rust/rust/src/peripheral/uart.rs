//! USART1 接收缓冲
//!
//! C 侧 UART 中断把收到的字节推入环形缓冲区，
//! Rust 主循环通过 `read_byte` 取走。
//! 单生产者（ISR）/ 单消费者（主循环），正好匹配 RingBuffer 的约束。
use crate::algo::ring_buffer::RingBuffer;
// peripheral/uart.rs
use core::fmt;
unsafe extern "C" {
    fn uart1_write(data: *const u8, len: u16);
}
/// 256 字节环形缓冲（实际可用 255 字节）
static UART1_RX: RingBuffer = RingBuffer::new();
/// 由 C 侧 UART 中断调用。
/// 缓冲区满返回 0（该字节被丢弃），成功返回 1。
#[unsafe(no_mangle)]
pub extern "C" fn rust_push_buffer(rx_byte: u8) -> u8 {
    if UART1_RX.push(rx_byte) { 1 } else { 0 }
}
/// 主循环读取一个字节
pub fn read_byte() -> Option<u8> {
    UART1_RX.pop()
}
/// 当前可读字节数
pub fn available() -> usize {
    UART1_RX.len()
}
/// 清空接收缓冲（仅主循环调用）
pub fn clear() {
    UART1_RX.clear();
}
pub struct UartWriter;
impl fmt::Write for UartWriter {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        let len = s.len().min(u16::MAX as usize) as u16;
        unsafe { uart1_write(s.as_ptr(), len) };
        Ok(())
    }
}
pub fn print(args: fmt::Arguments<'_>) {
    use fmt::Write;
    let _ = UartWriter.write_fmt(args);
}

#[macro_export]
macro_rules! u1println {
    ($($arg:tt)*) => {
        $crate::peripheral::uart::print(
            format_args!("{}\r\n", format_args!($($arg)*))
        )
    };
}
#[macro_export]
macro_rules! u1print {
    ($($arg:tt)*) => {
        $crate::peripheral::uart::print(
            format_args!("{}", format_args!($($arg)*))
        )
    };
}
