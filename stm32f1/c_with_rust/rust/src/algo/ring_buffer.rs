//! 256 字节环形缓冲区
//!
//! 算法与 C++ 版本一致：`head` 写指针、`tail` 读指针、`MASK` 取模，
//! 牺牲一个槽位区分空/满，所以 256 字节缓冲区实际可存 **255 字节**。
//!
//! 与 C++ 版 `volatile uint16_t head/tail` 的对应关系：
//! - Rust 没有 `volatile` 语义的共享可变量，这里用 `AtomicUsize` 代替；
//! - 原子操作同时提供内存序，保证“先写数据、再发布 head”的顺序；
//! - 方法改为 `&self`，这样可以直接放在 `static` 里被中断和主循环共享；
//! - `unsafe impl Sync` 成立的前提是 **严格单生产者/单消费者**：
//!   只有中断里调用 `push`，只有主循环里调用 `pop`。

use core::cell::UnsafeCell;
use core::sync::atomic::{AtomicUsize, Ordering};

/// 缓冲区大小，必须是 2 的幂
pub const CAPACITY: usize = 256;

const MASK: usize = CAPACITY - 1;

// 编译期保证 CAPACITY 是 2 的幂，否则 MASK 取模不成立
const _: () = assert!(CAPACITY.is_power_of_two());

pub struct RingBuffer {
    buf: UnsafeCell<[u8; CAPACITY]>,
    /// 写入位置，仅生产者（中断）修改
    head: AtomicUsize,
    /// 读取位置，仅消费者（主循环）修改
    tail: AtomicUsize,
}

// SAFETY: push/pop 只允许一个生产者、一个消费者并发访问；
// 生产者只写 head 及其槽位，消费者只写 tail 并读取已发布的槽位。
unsafe impl Sync for RingBuffer {}

impl RingBuffer {
    pub const fn new() -> Self {
        Self {
            buf: UnsafeCell::new([0; CAPACITY]),
            head: AtomicUsize::new(0),
            tail: AtomicUsize::new(0),
        }
    }

    /// 推入一个字节（中断里调用）。
    ///
    /// 缓冲区满时返回 `false`，该字节被丢弃。
    pub fn push(&self, byte: u8) -> bool {
        let head = self.head.load(Ordering::Relaxed);
        let next = (head + 1) & MASK;

        // Acquire 确保看到消费者已经释放出来的槽位
        if next == self.tail.load(Ordering::Acquire) {
            return false;
        }

        // SAFETY: 该槽位位于 [tail, head] 之外，消费者不会读取
        unsafe {
            (*self.buf.get())[head] = byte;
        }

        // Release：先写数据，再发布新的 head
        self.head.store(next, Ordering::Release);
        true
    }

    /// 弹出一个字节（主循环调用）。
    ///
    /// 缓冲区为空时返回 `None`。
    pub fn pop(&self) -> Option<u8> {
        let tail = self.tail.load(Ordering::Relaxed);
        let head = self.head.load(Ordering::Acquire);

        if tail == head {
            return None;
        }

        // SAFETY: tail != head，说明该槽位的数据已被生产者发布
        let byte = unsafe { (*self.buf.get())[tail] };

        // Release：读走数据后再释放槽位
        self.tail.store((tail + 1) & MASK, Ordering::Release);
        Some(byte)
    }

    pub fn is_empty(&self) -> bool {
        self.tail.load(Ordering::Acquire) == self.head.load(Ordering::Acquire)
    }

    pub fn is_full(&self) -> bool {
        let next = (self.head.load(Ordering::Relaxed) + 1) & MASK;
        next == self.tail.load(Ordering::Acquire)
    }

    /// 清空缓冲区（仅消费者/主循环调用，丢弃当前所有数据）
    pub fn clear(&self) {
        let head = self.head.load(Ordering::Acquire);
        self.tail.store(head, Ordering::Release);
    }

    /// 当前可读字节数
    pub fn len(&self) -> usize {
        let head = self.head.load(Ordering::Acquire);
        let tail = self.tail.load(Ordering::Acquire);
        (head + CAPACITY - tail) & MASK
    }
}

impl Default for RingBuffer {
    fn default() -> Self {
        Self::new()
    }
}
