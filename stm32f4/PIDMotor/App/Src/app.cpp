#include "app.h"
#include "motor.h"
#include "encoder.h" /* Encoder_Init() */
#include "control.h" /* 含 pid.h;extern PID motor_pid 定义在 control.cpp */
#include "bsp.h"     /* delay_ms() */
#include "shell.h"
#include "cli_motor.h"

/* ========== 初始化 ========== */
void App_Init(void)
{
    BSP_Init();     /* 使能 DWT 时基(delay_init)+ 串口接收 */
    Motor_Init();   /* PWM 就绪,电机停止 */
    Encoder_Init(); /* TIM2 编码器启动,测速窗口同步 */
    motor_pid.init(0.08f, 0.04f, 0.0f, -1000.0f, 1000.0f);

    Control_Init(); /* 武装 TIM6 10ms 控制中断 */
    Shell_Init_IT(); /* 武装 TIM7 10ms 串口输入处理中断 */

    printf("App init done\r\n");
    shell_init();
    motor_cli_register();
}

/* ========== 主循环 ========== */
void App_Loop(void)
{

}
