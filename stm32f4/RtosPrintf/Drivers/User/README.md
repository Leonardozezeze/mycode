# User 用户驱动目录

用户自定义外设/模块驱动统一放这里，**一个外设一对 `<device>.c/.h`**，平铺在 `Drivers/User/`，与参考工程布局一致：

| 文件                    | 内容                                            |
|-------------------------|-------------------------------------------------|
| `Drivers/User/<device>.h` | 类型定义 + 公开接口声明                        |
| `Drivers/User/<device>.c` | 实现，句柄/引脚配置在驱动内部管理              |

**约定**
- 驱动入口统一命名 `<device>_Init()`，风格对齐 `BSP_Init()`
- 编译由 CMakeLists 的 `Drivers/User/*.c` glob 自动纳入，新增 `.c` 文件无需改构建配置
- `Drivers/User` 已加入全局头文件搜索路径，Core / Drivers 之间可直接 `#include` 互引
- 板级接线类初始化在 `BSP_Init()` 调用；应用层设备在 `APP_Init()` 调用

示例：OLED 驱动 → `Drivers/User/oled.h` + `Drivers/User/oled.c`
