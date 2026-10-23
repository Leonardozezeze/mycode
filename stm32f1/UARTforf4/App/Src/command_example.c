/**
 * @file    command_example.c
 * @brief   命令模块使用示例 (仅供参考,不参与编译)
 * @note    此文件展示了如何使用 command.h 中定义的命令函数
 *
 * 使用方法:
 * 1. 在 main.c 或 app.c 中 #include "command.h"
 * 2. 在初始化时调用 Command_Init()
 * 3. 根据需要调用各个命令函数
 */

#if 0  /* 此文件不参与编译,仅作为文档示例 */

#include "command.h"
#include "bsp.h"

/* 示例:在 main() 或 App_Init() 中 */
void Example_Init(void)
{
    BSP_Init();           // 初始化板级支持
    Command_Init();       // 初始化命令模块

    // 查询 PID 状态
    Command_MotorPID_QueryStatus();
    delay_ms(100);
}

/* 示例 1:开启 PID 并设置速度 */
void Example_StartPID(void)
{
    // 开启 PID 控制
    Command_MotorPID_On();
    delay_ms(100);  // 等待 F4 响应

    // 设置目标速度为 1000
    Command_MotorPID_SetSpeed(1000);
}

/* 示例 2:动态调整速度 */
void Example_AdjustSpeed(void)
{
    // 加速
    Command_MotorPID_SetSpeed(1500);
    delay_ms(1000);

    // 减速
    Command_MotorPID_SetSpeed(800);
    delay_ms(1000);
}

/* 示例 3:整定 PID 参数 */
void Example_TunePID(void)
{
    // 先关闭 PID
    Command_MotorPID_Off();
    delay_ms(100);

    // 设置新的 PID 参数 (Kp=2.5, Ki=1.0, Kd=0.5)
    // 注意:参数需要放大 100 倍传递
    Command_MotorPID_SetParams(250, 100, 50);
    delay_ms(100);

    // 重新开启 PID
    Command_MotorPID_On();
}

/* 示例 4:急停处理 */
void Example_EmergencyStop(void)
{
    // 检测到紧急情况时
    Command_Motor_EmergencyStop();

    // 发送自定义错误信息 (调试用)
    Command_Send("error: emergency stop triggered at %d", HAL_GetTick());
}

/* 示例 5:在 main loop 中使用 */
void Example_MainLoop(void)
{
    static uint32_t last_cmd_time = 0;

    // 每 5 秒查询一次状态
    if (HAL_GetTick() - last_cmd_time > 5000) {
        Command_MotorPID_QueryStatus();
        last_cmd_time = HAL_GetTick();
    }

    // 根据 UART1 接收到的命令进行控制
    // ... (需要你自己实现命令解析逻辑)
}

/* 示例 6:发送自定义命令 */
void Example_CustomCommand(void)
{
    // 发送自定义格式的命令 (Command_Send 会自动添加 \r\n)
    Command_Send("motor config max_speed %d", 2000);
    Command_Send("motor config acceleration %d", 500);
    Command_Send("system reset");
}

#endif /* 不参与编译 */
