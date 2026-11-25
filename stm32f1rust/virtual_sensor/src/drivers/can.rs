use core::cell::Cell;
use embassy_stm32::can::{Frame, enums::BusError, frame::Envelope};
use embassy_sync::{
    blocking_mutex::{Mutex, raw::CriticalSectionRawMutex},
    channel::{Channel, TrySendError},
};

#[derive(Debug, Clone, Copy)]
pub struct CanBusErrorCounts {
    pub stuff: u32,
    pub form: u32,
    pub ack: u32,
    pub bit_recessive: u32,
    pub bit_dominant: u32,
    pub crc: u32,
    pub software: u32,
}

#[derive(Debug, Clone, Copy)]
pub struct CanStats {
    /// HAL 报告的总线错误次数。
    pub bus_error_events: u32,
    pub bus_errors: CanBusErrorCounts,
    pub last_bus_error: Option<&'static str>,
    /// 软件发送队列拒绝非阻塞发送的次数。
    pub tx_queue_full: u32,
}

pub struct CanLink {
    pub(crate) tx_queue: Channel<CriticalSectionRawMutex, Frame, 4>,
    pub(crate) rx_queue: Channel<CriticalSectionRawMutex, Result<Envelope, BusError>, 8>,
    stats: Mutex<CriticalSectionRawMutex, Cell<CanStats>>,
}

impl CanLink {
    pub(crate) const fn new() -> Self {
        Self {
            tx_queue: Channel::new(),
            rx_queue: Channel::new(),
            stats: Mutex::new(Cell::new(CanStats {
                bus_error_events: 0,
                bus_errors: CanBusErrorCounts {
                    stuff: 0,
                    form: 0,
                    ack: 0,
                    bit_recessive: 0,
                    bit_dominant: 0,
                    crc: 0,
                    software: 0,
                },
                last_bus_error: None,
                tx_queue_full: 0,
            })),
        }
    }

    pub fn try_send(&self, frame: Frame) -> Result<(), TrySendError<Frame>> {
        let result = self.tx_queue.try_send(frame);
        if result.is_err() {
            self.stats.lock(|cell| {
                let mut stats = cell.get();
                stats.tx_queue_full = stats.tx_queue_full.saturating_add(1);
                cell.set(stats);
            });
        }
        result
    }

    pub async fn send(&self, frame: Frame) {
        self.tx_queue.send(frame).await
    }

    pub async fn receive(&self) -> Result<Envelope, BusError> {
        self.rx_queue.receive().await
    }

    pub fn stats(&self) -> CanStats {
        self.stats.lock(Cell::get)
    }

    pub(crate) fn record_bus_error(&self, error: &BusError) {
        self.stats.lock(|cell| {
            let mut stats = cell.get();
            let (name, count) = match error {
                BusError::Stuff => ("stuff", &mut stats.bus_errors.stuff),
                BusError::Form => ("form", &mut stats.bus_errors.form),
                BusError::Acknowledge => ("ack", &mut stats.bus_errors.ack),
                BusError::BitRecessive => ("bit_recessive", &mut stats.bus_errors.bit_recessive),
                BusError::BitDominant => ("bit_dominant", &mut stats.bus_errors.bit_dominant),
                BusError::Crc => ("crc", &mut stats.bus_errors.crc),
                BusError::Software => ("software", &mut stats.bus_errors.software),
            };
            *count = count.saturating_add(1);
            stats.bus_error_events = stats.bus_error_events.saturating_add(1);
            stats.last_bus_error = Some(name);
            cell.set(stats);
        });
    }
}
