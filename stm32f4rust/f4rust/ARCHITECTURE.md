# f4rust 架构说明

STM32F407 嵌入式 Rust 固件。本文档记录分层约定、开发契约和路线图；
代码本身的作用见各源文件注释。

## 目录结构

```text
src/
├── main.rs        # 入口：App::init().run()
├── app.rs         # 业务层：App 持有运行期状态，主循环调度
├── board.rs       # BSP：时钟、引脚映射、外设实例化
├── drivers.rs     # 驱动层：器件与内核设施驱动
│   ├── dwt.rs     # DWT 周期计数器：系统时基（u64 µs）+ 忙等延时
│   └── uart.rs    # UartLink：USART1 阻塞发送（RX 待接中断 + ringbuf）
├── control.rs     # 控制律层（纯算法）
│   └── pid.rs     # 增量式 PID：输出累加 + 限幅，天然抗积分饱和
├── comm.rs        # 通信基础设施层（纯逻辑）
│   ├── ringbuf.rs # 环形缓冲区（SPSC、无锁）
│   └── proto.rs   # 字节流 → 帧 的解析状态机
└── algo.rs        # 通用构件层（非业务领域的纯逻辑）
    └── sched.rs   # PeriodicTask：周期任务原语
```

## 分层与依赖方向

```text
app ──→ drivers ──→ HAL 外设实例（由 board 生产）
 ├───→ control（控制律，零硬件依赖）
 ├───→ comm   （缓冲/协议，零硬件依赖）
 └───→ algo   （通用构件，零硬件依赖）
```

- 依赖只从上往下，**control、comm、algo 禁止引入 HAL/PAC**，保证可移植；
- drivers 只封装"怎么操作器件"，不做业务决策；
- app 只做业务决策和调度，不直接碰寄存器；
- 例外：`board::init()` 上电时把 DWT/DCB 句柄交给 `drivers::dwt`
  （时基是全局设施，见契约 3）。

### 新代码放哪（判断规则）

| 代码性质 | 去处 |
|---|---|
| 碰寄存器、引脚、时钟 | `board.rs` |
| 封装某个器件或内核设施的操作 | `drivers/`（如 motor.rs、uart.rs、dwt.rs） |
| 控制律、滤波等计算 | `control/` |
| 字节缓冲、帧协议、校验 | `comm/` |
| 不属于任何业务领域的通用构件（调度原语等） | `algo/`（准入标准见 algo.rs 头注释） |
| 业务决策、流程调度 | `app.rs` |

## 关键契约

### 1. Board / DeviceParts 双通道上交，app 不碰裸外设

`board::init()` 把 PAC 寄存器堆变成**配置完毕**的资源，按准入测试分两路上交：

> **直接交给 App 使用的 → `Board`；需先经 driver 包装的 → `DeviceParts`。**

- `Board`：琐碎资源（LED、clocks），app 直接使用；
- `DeviceParts`：器件零件（uart tx/rx，以后的 pwm 通道……），
  由 `App::init()` 组装成器件对象（UartLink、Motor……），app 之后只跟 drivers 打交道；
- 配置参数（波特率、引脚 AF、PWM 频率）是"板子事实"，全部在 board.rs 定死；
- `App::init()` 只做"零件 → 器件"的组装，也是资源移交中断侧（契约 2）的唯一移交点。

```rust
// App::init() 形态
let (board, parts) = board::init();
let DeviceParts { uart1_tx, uart1_rx } = parts;
let uart = UartLink::new(uart1_tx);   // 以后跑 comm::proto 协议
// 电机来了：let motor = Motor::new(parts.pwm_ch1, parts.pwm_ch2, parts.dir_pin);
Self { board, uart, uart1_rx, pid, ... }
```

### 2. 中断侧外设是例外：只通过 ringbuf 见面

UART RX 这类中断驱动的外设由 `#[interrupt]` 侧 static 接管，
app 拿不到所有权，也不应该拿。数据通路：

```text
USART1 中断 → uart 驱动把字节压入 comm::ringbuf（生产者）
主循环      → app 取出字节（消费者）→ comm::proto 解析成帧 → 业务
```

app 与中断之间不共享外设，只通过 ringbuf 交换字节；
单生产者-单消费者场景，不需要锁。

### 3. 时基与主循环节拍：drivers::dwt + deadline 驱动

- 系统唯一时基是 `drivers::dwt::DWT`：硬件 CYCCNT 32 位计数
  （168 MHz 下约 25.6 s 回绕）+ 软件回绕累加折叠成 64 位，
  对外 `uptime_us() / uptime_ms()` 单调不回绕，调用方直接普通比较/减法；
- **隐含契约**：相邻两次 `uptime` 调用间隔必须小于一圈回绕（约 25.6 s），
  否则漏掉的回绕补不回来。主循环每拍都读，天然满足；
- 每微秒周期数在 `DWT::init()` 时由 HCLK 推得，时钟配置的单一来源仍是 board.rs；
- 主循环**非阻塞**：任务节拍由 `algo::sched::PeriodicTask` 管理，
  `poll(now)` 到点才执行一拍"读输入 → 跑控制 → 写输出"，
  加功能往三步里填，不改骨架。每个周期任务一个 `PeriodicTask` 字段，
  主循环每圈读一次时间戳、依次 poll 各任务；
- `PeriodicTask` 锚定实际触发时刻（实际周期 ≈ period + 任务耗时），
  `poll` 返回实测间隔 `Option<elapsed_ms>`，PID 的 dt 直接用它；
- 系统不保留阻塞延时外设：SysTick 完全空闲，TIM 全部空余；
  临时需要忙等延时时用 `DWT::delay_us / delay_ms`。

## 路线图

- [ ] 确定帧格式（与 Qt 上位机现有协议对齐）→ 实现 `comm/proto.rs`
- [ ] 实现 `comm/ringbuf.rs`（SPSC 环形缓冲）
- [x] `drivers/uart.rs`：阻塞发送（UartLink，USART1 PA9/PA10，115200-8N1）
- [ ] UART RX：中断接收，移交中断侧 static，接入 `comm::ringbuf`（契约 2）
- [x] 非阻塞主循环：drivers::dwt 时基（u64 微秒）+ deadline 驱动（控制周期暂为 500ms 占位）
- [ ] `drivers/motor.rs`：PWM + 方向，对外 `set_speed(f32)`
- [ ] drivers 就位、占位字段被真实使用后，移除 main.rs 的 `#![allow(dead_code)]`
