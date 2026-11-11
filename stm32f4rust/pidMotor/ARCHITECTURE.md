# f4rust 架构说明

STM32F407 嵌入式 Rust 固件。本文档记录分层约定、开发契约和路线图；
代码本身的作用见各源文件注释。

## 目录结构

```text
src/
├── main.rs        # 入口：App::init().run()
├── runtime.rs     # 运行期模块入口
├── runtime/
│   ├── app.rs     # 业务层：持有 Devices、PID 和任务，主循环调度
│   ├── board.rs   # BSP：时钟、引脚、外设初始化和驱动组装
│   ├── config.rs  # 电机参数、测速周期、传感器字节序与处理预算
│   ├── panic.rs   # panic 处理
│   └── sched.rs   # PeriodicTask：周期任务原语
├── drivers.rs     # 驱动层：器件与内核设施驱动
│   ├── dwt.rs     # DWT 周期计数器：系统时基（u64 µs）+ 忙等延时
│   ├── uart.rs    # UartLink：TX/RX 中断与队列、错误统计
│   ├── motor.rs   # Motor：PWM、方向和短路刹车
│   └── enc.rs     # Encoder：16 位编码器原始计数、带符号计数差和回绕处理
├── control.rs     # 控制律层（纯算法）
│   └── pid.rs     # 增量式 PID：输出累加 + 限幅，天然抗积分饱和
└── comm.rs        # 通信基础设施层（纯逻辑）
    ├── ringbuf.rs # 环形缓冲区（SPSC、无锁）
    └── proto.rs   # UART5 传感器逐字节解析，输出两个 float
```

## 分层与依赖方向

```text
app ──→ board::init() → Devices（LED 和已包装好的设备）
 ├───→ drivers（运行期设备操作）
 ├───→ control（控制律，零硬件依赖）
 ├───→ comm（缓冲/协议，零硬件依赖）
 ├───→ runtime::config（电机参数）
 └───→ runtime::sched（周期调度）
board ──→ HAL/PAC（硬件配置）
      └─→ drivers（设备组装）
```

- 依赖只从上往下，**control、comm、runtime::sched 禁止引入 HAL/PAC**，保证可移植；
- drivers 只封装"怎么操作器件"，不做业务决策；
- app 只做业务决策和调度，不直接碰寄存器；
- board 初始化硬件后调用驱动构造器，配置所需的具体设备类型也在 board 中定义；
- `board::init()` 上电时把 DWT/DCB 句柄交给 `drivers::dwt`（见契约 3）。

### 新代码放哪（判断规则）

| 代码性质 | 去处 |
|---|---|
| 板级时钟、引脚映射、外设配置和驱动组装 | `runtime/board.rs` |
| 封装某个器件或内核设施的操作 | `drivers/`（如 motor.rs、uart.rs、dwt.rs） |
| 控制律、滤波等计算 | `control/` |
| 字节缓冲、帧协议、校验 | `comm/` |
| 电机 PWM 频率、编码器线数、减速比和测速周期 | `runtime/config.rs` |
| 周期调度原语 | `runtime/sched.rs` |
| 业务决策、流程调度 | `runtime/app.rs` |

## 关键契约

### 1. Devices：board 完成硬件配置和设备组装

`board::init()` 初始化 HAL 外设，再直接调用驱动构造器，返回 `Devices`。

- `Devices`：初始化好的 LED 和已经包装好的 `Motor`、`UartLink`、`Encoder`，App 可直接使用；
- 不再单独保留 `Board` 结构体；时钟信息在 board 初始化期间使用，运行期需要时再显式返回；
- 波特率、引脚 AF 和时钟配置放在 `runtime/board.rs`，设备的具体类型直接写在 `Devices` 字段中；
- 电机 PWM 频率和编码器机械参数集中在 `runtime/config.rs`；board 使用 PWM 频率配置硬件，App 使用换算参数计算转速；
- drivers 保持泛型，不依赖 board 或 App；
- `App` 通过 `devices: Devices` 持有设备，`App::init()` 初始化 PID 和周期任务；
- 中断资源的移交由 board 调用驱动构造器触发，资源泊入和中断开启顺序由驱动保证。

```rust
let devices = board::init();
// 初始化 pid、heartbeat_task 等业务状态……
Self { devices, pid, heartbeat_task }

// 主循环中通过设备集合使用硬件
self.devices.green_led.toggle();
self.devices.motor.set_pwm(1000);
```

### 2. 中断侧外设是例外：只通过 ringbuf 见面

board 调用 `uart::uart1()` 组装串口时，将 TX/RX 都移交给驱动的中断侧。
串口硬件句柄由 `#[interrupt]` 侧 static 接管，
app 拿不到所有权，也不应该拿。数据通路：

```text
RX：USART1 中断 → 接收队列 → 主循环/Shell 消费（尚未接入）
TX：主循环 → 发送队列 → USART1 TXE 中断 → 串口硬件
```

app 与中断之间不共享外设，只通过 ringbuf 交换字节；
每条队列通过 `RingBuffer::split(&mut self)` 拆成唯一的 `Producer` / `Consumer`。
原始 RingBuffer 不公开读写方法；句柄不可复制、不可共享引用跨上下文，修改必须使用 `&mut self`。
句柄借用期间，编译器禁止再次拆分或移动底层缓冲区。

| 队列 | UartLink 持有 | USART 中断持有 |
|---|---|---|
| TX | Producer（入队） | Consumer（取出发送） |
| RX | Consumer（供 Shell 读取） | Producer（收到后入队） |

串口构造器用 `cortex_m::singleton!` 获取一次性的静态缓冲区独占引用，再拆分、分配句柄；
硬件句柄和中断侧端点一同存入 IsrState，主循环不再持有原始队列引用。

USART1 用于二进制遥测回传；UART5 用于传感器接收，配置为 PC12 TX / PD2 RX（AF8）、115200、8N1。
两路串口各有独立的队列、统计和中断泊位，复用 UartLink 与中断处理逻辑。
UART5 的字节由 `devices.uart5.read_byte()` 读取，接收中断只入队，不解析。
传感器帧固定为 10 字节：`0xFF + 4 字节小端 float + 4 字节小端 float + 0xEE`。
`comm::proto::SensorParser` 逐字节处理，在指定位置验证帧头/帧尾；中间数据允许包含相同标记字节。
若候选帧尾不匹配，保留候选帧内下一个帧头开始的字节，继续尝试同步。
App 每轮最多消费 `SENSOR_RX_BUDGET`（64）字节，不等待新数据；解析状态跨循环保留，完整帧更新 `sensor_frame`。
每个有效传感器帧到达时，通过 USART1 回传 18 字节：`FF | b f32 | i f32 | 实测 RPM f32 | 目标 RPM f32 | EE`，均为小端。
App 调用 proto.rs 的 encode_telemetry 编码，回传使用最近一次测速结果；发送队列满时整帧跳过，不阻塞控制。
USART1 不混入正常文本日志，心跳只翻灯；panic 诊断除外。
`sensor_frame` 在首帧前为 None，value1/value2 分别表示破碎率 b 与含杂率 i；非有限值帧不参与业务处理。
当前协议无校验和，只能检查帧边界，无法检测所有载荷错误。

- `send()` / `send_str()` 只复制入队，不等待线上发送完成；队列满时整条拒绝并返回 `SendError::QueueFull`，最大单次长度为 255 字节；
- `writeln!` 先在栈上格式化完整消息，再整条入队；消息超过 255 字节或队列不足时返回 `fmt::Error`；
- 发布发送队列后 pend USART 中断，中断按 TXE 状态逐字节发送，队列清空后关闭 TXEIE；
- `WouldBlock` 时保留待发字节，下一次中断继续尝试；单次中断最多进行 32 次 RX 读取和一次 TX 尝试，不自旋等待；
- 接收线路错误由 HAL 读取时清除，驱动区分记录 overrun、帧错误、校验错误、噪声与其他错误；每次读取只统计 HAL 返回的那个错误类型；
- `stats()` 返回累计统计，包括 RX 队列满丢弃字节数、TX 入队失败次数及发送错误次数；
- panic 打印保留独立的轮询发送路径，不依赖正常发送队列或中断。

### 3. 时基与主循环节拍：drivers::dwt + deadline 驱动

- 系统唯一时基是 `drivers::dwt::DWT`：硬件 CYCCNT 32 位计数
  （168 MHz 下约 25.6 s 回绕）+ 软件回绕累加折叠成 64 位，
  对外 `uptime_us() / uptime_ms()` 单调不回绕，调用方直接普通比较/减法；
- **隐含契约**：相邻两次 `uptime` 调用间隔必须小于一圈回绕（约 25.6 s），
  否则漏掉的回绕补不回来。主循环每拍都读，天然满足；
- 每微秒周期数在 `DWT::init()` 时由 HCLK 推得，时钟配置的单一来源仍是 board.rs；
- 主循环**非阻塞**：任务节拍由 `runtime::sched::PeriodicTask` 管理，
  `poll(now)` 到点才执行一拍"读输入 → 跑控制 → 写输出"，
  加功能往三步里填，不改骨架。每个周期任务一个 `PeriodicTask` 字段，
  主循环每圈读一次时间戳、依次 poll 各任务；
- `PeriodicTask::new_at()` 可将首次任务起点对齐到采样基准；
- `PeriodicTask` 锚定实际触发时刻（实际周期 ≈ period + 任务耗时），
  `poll` 返回实测间隔 `Option<elapsed_ms>`，PID 的 dt 直接用它；
- 系统不保留阻塞延时外设：SysTick 空闲，TIM3 用于电机 PWM，TIM4 用于编码器；
  临时需要忙等延时时用 `DWT::delay_us / delay_ms`。

### 4. 编码器采样与测速任务

- TIM4 的 CH1/CH2 使用 PD12/PD13（AF2），分别接编码器 A/B 相，在 board 中初始化后包装为 `Encoder<TIM4>`；
- Encoder 只持有 QEI 和上次计数，不依赖 DWT、config 或电机机械参数；
- `count()` 只读原始计数；`delta_count()` 返回相对上次调用的带符号计数差并更新基准，不清零硬件计数器；
- `runtime/config.rs` 保存 11 PPR、四倍频、减速比 21.3，输出轴每转 937.2 计数；倍频常量用于换算，不配置硬件；
- App 的测速任务默认每 10 ms 采样一次，使用 `poll()` 返回的实际间隔换算转速：`RPM = delta × 60000 / elapsed_ms / COUNTS_PER_REV`；
- App 初始化时同步计数基准与任务起点，计算结果保存在 `motor_speed_rpm` 中供 PID、遥测回传共用；
- TIM4 为 16 位计数器，相邻采样实际位移须在 (-32768, 32768) 计数内，才能正确还原回绕与方向；
- 仅测速任务调用 `delta_count()`，其他业务复用采样结果，避免相互消耗计数差。

### 5. 破碎率与电机闭环业务

- App 收到有效帧后更新目标：b 在 [2.0, 5.0] 内线性反比映射到 [250, 50] RPM，越界取边界；首帧前目标为 0；
- 10 ms 测速任务以实际采样间隔更新 PID，并调用 Motor::set_pwm；初始化将电机置为零 PWM 刹车状态；
- 配置集中在 runtime/config.rs：Kp=0.3，Ki=20/s，Kd=0；Ki 等效于旧实现每 10 ms 累加的 0.2；
- PID 输出保持旧业务的 [-1000, 1000] 单位，再按 Motor::max_duty() 缩放为实际 PWM 计数，1000 对应满占空比；
- 绿灯每 1000 ms 翻转一次，不发送心跳文本。

## 路线图

- [x] UART5 传感器帧解析：帧头/尾 + 两个小端 float，主循环按预算消费
- [ ] Shell 帧格式与命令解析
- [ ] 实现 `comm/ringbuf.rs`（SPSC 环形缓冲）
- [x] `drivers/uart.rs`：非阻塞 TX 入队、TXE 中断发送与接收错误统计（USART1 PA9/PA10，115200-8N1）
- [x] UART RX：中断接收，移交中断侧 static，接入 `comm::ringbuf`（契约 2）
- [ ] Shell 消费接收队列与命令解析
- [x] 非阻塞主循环：drivers::dwt 时基（u64 微秒）+ deadline 驱动（控制周期暂为 500ms 占位）
- [ ] `drivers/motor.rs`：PWM + 方向，对外 `set_speed(f32)`
- [ ] drivers 就位、占位字段被真实使用后，移除 main.rs 的 `#![allow(dead_code)]`
