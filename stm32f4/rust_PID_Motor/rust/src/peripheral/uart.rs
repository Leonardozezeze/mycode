//! UART 收发模块（USART1 / UART4）
//!
//! C 侧 UART 中断把收到的字节推入环形缓冲区，Rust 侧用
//! `Uart::new(UartKind::…)` 拿到对应串口的读写接口。
//! 单生产者（ISR）/ 单消费者（主循环），正好匹配 RingBuffer 的约束。

use crate::algo::ring_buffer::RingBuffer;
use core::fmt;

/// 串口号
#[derive(Clone, Copy)]
pub enum UartKind {
    Uart1,
    Uart4,
}

/// 一个串口实例：绑定一个环形接收缓冲和对应的 C 侧写函数
#[derive(Clone, Copy)]
pub struct Uart {
    kind: UartKind,
}

unsafe extern "C" {
    fn uart1_write(data: *const u8, len: u16);
    fn uart4_write(data: *const u8, len: u16);
}

/// 256 字节环形缓冲（实际可用 255 字节）
static UART1_RX: RingBuffer = RingBuffer::new();
static UART4_RX: RingBuffer = RingBuffer::new();

/// 由 C 侧 UART1 中断调用。
/// 缓冲区满返回 0（该字节被丢弃），成功返回 1。
#[unsafe(no_mangle)]
pub extern "C" fn rust_push_buffer(rx_byte: u8) -> u8 {
    if UART1_RX.push(rx_byte) { 1 } else { 0 }
}

/// 由 C 侧 UART4 中断调用。
/// 缓冲区满返回 0（该字节被丢弃），成功返回 1。
#[unsafe(no_mangle)]
pub extern "C" fn rust_push_buffer4(rx_byte: u8) -> u8 {
    if UART4_RX.push(rx_byte) { 1 } else { 0 }
}

impl Uart {
    pub const fn new(kind: UartKind) -> Self {
        Self { kind }
    }

    fn rx(&self) -> &'static RingBuffer {
        match self.kind {
            UartKind::Uart1 => &UART1_RX,
            UartKind::Uart4 => &UART4_RX,
        }
    }

    /// 主循环读取一个字节
    pub fn read_byte(&self) -> Option<u8> {
        self.rx().pop()
    }

    /// 当前可读字节数
    pub fn available(&self) -> usize {
        self.rx().len()
    }

    /// 清空接收缓冲（仅主循环调用）
    pub fn clear(&self) {
        self.rx().clear();
    }

    /// 发送原始字节（一般用 `print` / `u1println!` 更方便）
    pub fn send_data(&self, data: *const u8, len: u16) {
        unsafe {
            match self.kind {
                UartKind::Uart1 => uart1_write(data, len),
                UartKind::Uart4 => uart4_write(data, len),
            }
        }
    }

    /// 格式化输出（不自动换行）
    pub fn print(&self, args: fmt::Arguments<'_>) {
        use fmt::Write;
        let mut w = *self;
        let _ = w.write_fmt(args);
    }
}

impl fmt::Write for Uart {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        let len = s.len().min(u16::MAX as usize) as u16;
        unsafe {
            match self.kind {
                UartKind::Uart1 => uart1_write(s.as_ptr(), len),
                UartKind::Uart4 => uart4_write(s.as_ptr(), len),
            }
        }
        Ok(())
    }
}

/// 供宏使用的自由函数：按串口号格式化输出
pub fn print(kind: UartKind, args: fmt::Arguments<'_>) {
    Uart::new(kind).print(args);
}

#[macro_export]
macro_rules! u1println {
    ($($arg:tt)*) => {
        $crate::peripheral::uart::print(
            $crate::peripheral::uart::UartKind::Uart1,
            format_args!("{}\r\n", format_args!($($arg)*))
        )
    };
}
#[macro_export]
macro_rules! u1print {
    ($($arg:tt)*) => {
        $crate::peripheral::uart::print(
            $crate::peripheral::uart::UartKind::Uart1,
            format_args!("{}", format_args!($($arg)*))
        )
    };
}
#[macro_export]
macro_rules! u4println {
    ($($arg:tt)*) => {
        $crate::peripheral::uart::print(
            $crate::peripheral::uart::UartKind::Uart4,
            format_args!("{}\r\n", format_args!($($arg)*))
        )
    };
}
#[macro_export]
macro_rules! u4print {
    ($($arg:tt)*) => {
        $crate::peripheral::uart::print(
            $crate::peripheral::uart::UartKind::Uart4,
            format_args!("{}", format_args!($($arg)*))
        )
    };
}
