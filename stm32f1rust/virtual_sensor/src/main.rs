#![no_main]
#![no_std]
#![allow(dead_code)]
mod comm;
mod control;
mod drivers;
mod runtime;
/// Embassy 创建线程模式执行器，main 本身就是第一个异步任务。
#[embassy_executor::main]
async fn main(spawner: embassy_executor::Spawner) {
    runtime::app::App::init().run(spawner).await
}
