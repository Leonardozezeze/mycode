#![no_main]
#![no_std]
#![allow(dead_code)]
mod algo;
mod app;
mod board;
mod comm;
mod control;
mod drivers;
mod panic;
/// 入口只做一件事：初始化 App，然后交还给它的主循环。
#[cortex_m_rt::entry]
fn main() -> ! {
    app::App::init().run()
}
