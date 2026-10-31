//! 应用逻辑层

pub mod app;

// 重新导出，方便外部调用
pub use app::{app_init, app_run};
