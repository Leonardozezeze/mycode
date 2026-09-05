# Driver 模块驱动目录

用户自定义外设/模块驱动统一放这里，**一个外设一对 `.c/.h`**，命名与工程风格对齐：

| 目录        | 内容                                              |
|-------------|---------------------------------------------------|
| `Driver/Inc`| `<device>.h` —— 类型定义 + 公开接口声明            |
| `Driver/Src`| `<device>.c` —— 实现，句柄/引脚配置在驱动内部管理  |

**约定**
- 驱动入口统一命名 `<device>_Init()`，风格对齐 `BSP_Init()`
- 编译由 CMakeLists 的 `Driver/Src/*.c` glob 自动纳入，新增 `.c` 文件无需改构建配置
- `Driver/Inc` 已加入全局头文件搜索路径，Core/Driver 之间可直接 `#include` 互引
- 板级接线类初始化在 `BSP_Init()` 调用；应用层设备在 `APP_Init()` 调用

示例：OLED 驱动 → `Driver/Inc/oled.h` + `Driver/Src/oled.c`
