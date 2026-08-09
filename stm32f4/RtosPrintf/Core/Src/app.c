/**
 * @file    app.c
 * @brief   应用层:FreeRTOS 任务 + 定时器回调
 */

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "app.h"

#include <stdio.h>


static void vLedTask1(void *pvParameters);
static void vLedTask2(void *pvParameters);

/* 应用层初始化:创建全部任务 */
void APP_Init(void)
{
    xTaskCreate(vLedTask1,    "LED1",    128, NULL, 2, NULL);
    xTaskCreate(vLedTask2,    "LED2",    128, NULL, 1, NULL);
}

/* FreeRTOS tick 钩子:保持 HAL_GetTick() 前进。
 * FreeRTOS 占用了 SysTick,否则 uwTick 永远为 0,HAL_GetTick() 停摆。 */
void vApplicationTickHook(void)
{
    HAL_IncTick();
}

static volatile uint32_t tim7_ms_tick = 0;   /* 1ms 软件节拍,ISR 内累加 */

/* TIM7 1ms 周期中断回调:硬件定时器 ISR 示例。
 * 该中断优先级(2)高于 FreeRTOS 内核,ISR 内禁止调用 FreeRTOS API。 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == &htim7) {
        tim7_ms_tick++;
    }
}

/* LED 闪烁任务(PF9) */
static void vLedTask1(void *pvParameters)
{
    (void)pvParameters;
    while (1) {
        HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_9);
        printf("LED1 blink, tick = %lu\r\n", (unsigned long)HAL_GetTick());
        vTaskDelay(pdMS_TO_TICKS(900));
    }
}

/* LED 闪烁任务(PF10) */
static void vLedTask2(void *pvParameters)
{
    (void)pvParameters;
    while (1) {
        HAL_GPIO_TogglePin(GPIOF, GPIO_PIN_10);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
