/**
 * @file    main.c
 * @brief   启动骨架:BSP_Init(硬件) -> APP_Init(应用) -> 调度器
 */

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "app.h"

int main(void)
{
    BSP_Init();   /* 硬件初始化:时钟 + 外设(见 bsp.c) */
    APP_Init();   /* 应用初始化:创建任务(见 app.c) */

    vTaskStartScheduler();

    /* 正常情况下到不了这里 */
    while (1) {}
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}
