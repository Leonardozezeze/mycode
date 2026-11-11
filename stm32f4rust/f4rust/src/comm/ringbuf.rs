//! 单生产者、单消费者环形缓冲区：split() 将操作权限拆成两个独占句柄。
//! 256 个槽位保留一个区分空/满，因此实际容量为 255 字节。

use core::{
    cell::{Cell, UnsafeCell},
    marker::PhantomData,
    sync::atomic::{AtomicUsize, Ordering},
};

pub const CAPACITY: usize = 256;
const MASK: usize = CAPACITY - 1;
const _: () = assert!(CAPACITY >= 2 && CAPACITY.is_power_of_two());

pub struct RingBuffer {
    buf: UnsafeCell<[u8; CAPACITY]>,
    head: AtomicUsize,
    tail: AtomicUsize,
}

// SAFETY: 数据访问只能经由 split 创建的独占 Producer/Consumer。
// 句柄不可复制且 !Sync，修改要求 &mut self；每种角色最多一个操作者。
// Release/Acquire 保证槽位写入后才发布，读取完后才允许覆盖。
// 维护约束：不得新增能通过共享引用写入槽位或修改索引的公共接口，
// 也不得公开底层存储/原子索引或允许复制、并发操作同一角色的句柄。
unsafe impl Sync for RingBuffer {}

/// 唯一写入者。可以移动到另一执行上下文，但不能共享引用并发写入。
pub struct Producer<'a> {
    buffer: &'a RingBuffer,
    _not_sync: PhantomData<Cell<()>>,
}

/// 唯一读取者。可以移动到另一执行上下文，但不能共享引用并发读取。
pub struct Consumer<'a> {
    buffer: &'a RingBuffer,
    _not_sync: PhantomData<Cell<()>>,
}

impl RingBuffer {
    pub const fn new() -> Self {
        Self {
            buf: UnsafeCell::new([0; CAPACITY]),
            head: AtomicUsize::new(0),
            tail: AtomicUsize::new(0),
        }
    }

    /// 拆出共享同一底层存储的两个句柄，不复制缓冲区。
    /// 句柄使用期间独占借用整个 RingBuffer，不能再次拆分或移动缓冲区。
    pub fn split(&mut self) -> (Producer<'_>, Consumer<'_>) {
        (
            Producer {
                buffer: self,
                _not_sync: PhantomData,
            },
            Consumer {
                buffer: self,
                _not_sync: PhantomData,
            },
        )
    }
}

impl Default for RingBuffer {
    fn default() -> Self {
        Self::new()
    }
}

impl Producer<'_> {
    /// 推入一个字节；满时返回 false，不改变队列。
    pub fn push(&mut self, byte: u8) -> bool {
        self.try_push_slice(core::slice::from_ref(&byte))
    }

    /// 整批入队；空间不足时返回 false，不写入任何字节。
    pub fn try_push_slice(&mut self, data: &[u8]) -> bool {
        if data.is_empty() {
            return true;
        }
        let buffer = self.buffer;
        let head = buffer.head.load(Ordering::Relaxed);
        let tail = buffer.tail.load(Ordering::Acquire);
        let available = (tail + CAPACITY - head - 1) & MASK;
        if data.len() > available {
            return false;
        }

        for (offset, &byte) in data.iter().enumerate() {
            // SAFETY: 独占 Producer 只写已确认空闲的槽位，Consumer 不会读取这些槽位。
            unsafe { *buffer.buf.get().cast::<u8>().add((head + offset) & MASK) = byte };
        }
        buffer
            .head
            .store((head + data.len()) & MASK, Ordering::Release);
        true
    }

    /// 满状态的一次观察；消费者可能在返回后立即释放空间。
    /// 实际能否入队以 push/try_push_slice 的返回值为准。
    pub fn is_full(&self) -> bool {
        let next = (self.buffer.head.load(Ordering::Relaxed) + 1) & MASK;
        next == self.buffer.tail.load(Ordering::Acquire)
    }
}

impl Consumer<'_> {
    pub fn pop(&mut self) -> Option<u8> {
        let buffer = self.buffer;
        let tail = buffer.tail.load(Ordering::Relaxed);
        let head = buffer.head.load(Ordering::Acquire);
        if tail == head {
            return None;
        }

        // SAFETY: 数据已发布，且只有此 Consumer 会读取并释放当前槽位。
        let byte = unsafe { *buffer.buf.get().cast::<u8>().add(tail) };
        buffer.tail.store((tail + 1) & MASK, Ordering::Release);
        Some(byte)
    }

    /// 空状态的一次观察；生产者可能在返回后立即发布新数据。
    /// 实际能否读取以 pop 的返回值为准。
    pub fn is_empty(&self) -> bool {
        self.buffer.tail.load(Ordering::Relaxed) == self.buffer.head.load(Ordering::Acquire)
    }

    /// 丢弃本次 Acquire 读取 head 所观察到的已发布数据，时间复杂度 O(1)。
    /// 未发布的写入不会被计入；并发时在该次观察之后发布的数据会被保留，
    /// 因此返回时队列不保证为空。此方法不读取或改写任何数据槽。
    pub fn clear(&mut self) {
        let head = self.buffer.head.load(Ordering::Acquire);
        self.buffer.tail.store(head, Ordering::Release);
    }

    /// 可读字节数的一次观察；生产者并发追加时，结果可能立即过时。
    /// Consumer 的独占借用保证观察期间 tail 不会被另一消费者修改。
    pub fn len(&self) -> usize {
        let head = self.buffer.head.load(Ordering::Acquire);
        let tail = self.buffer.tail.load(Ordering::Relaxed);
        (head + CAPACITY - tail) & MASK
    }
}
