//! 协议接收入口。目前只记录原始数据，尚未定义帧格式或命令。
//! 不做回显。后续解析器在 on_data / on_error 处接入，不把 DMA 数据块当协议帧。

use heapless::Vec;

/// 最近一次接收状态，可在调试器中查看；不保存完整历史数据。
pub struct RxState<const N: usize> {
    pub received_bytes: u64,
    pub received_chunks: u32,
    pub error_events: u32,
    pub last_chunk: Vec<u8, N>,
}

impl<const N: usize> RxState<N> {
    pub const fn new() -> Self {
        Self {
            received_bytes: 0,
            received_chunks: 0,
            error_events: 0,
            last_chunk: Vec::new(),
        }
    }

    pub fn on_data(&mut self, data: &[u8]) {
        self.received_bytes = self.received_bytes.wrapping_add(data.len() as u64);
        self.received_chunks = self.received_chunks.wrapping_add(1);
        self.last_chunk.clear();
        // 只保留前 N 字节作诊断，计数仍覆盖本次所有字节。
        let _ = self
            .last_chunk
            .extend_from_slice(&data[..data.len().min(N)]);
    }

    pub fn on_error(&mut self) {
        self.error_events = self.error_events.wrapping_add(1);
        self.last_chunk.clear();
        // 接收流已不连续，未来协议解析器应在这里丢弃未完成帧。
    }
}
