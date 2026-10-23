#include "app.h"
#include "command.h"

/* ========== 私有变量 ========== */

/* ========== 初始化 ========== */
void App_Init(void)
{
    BSP_Init();
    Command_MotorPID_Off();
    delay_ms(10);
    Command_MotorPID_On();
    delay_ms(10);
    Command_MotorPID_SetParams(0.7, 0.3, 0);
    delay_ms(10);
    Command_MotorPID_SetSpeed(100);
    printf("App init done\r\n");
}

/* ========== 主循环 ========== */
void App_Loop(void)
{
    HAL_GPIO_TogglePin(LED_GPIO_PORT,LED_GPIO_PIN);
    delay_ms(1000);
}
