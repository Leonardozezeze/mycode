/**
 * @file    app.c
 * @brief   应用层:FreeRTOS 任务 + 定时器回调
 *
 * CANopen RTOS 移植:与裸机模板同构——
 *   - TIM7 1ms ISR  = "timer 线程":canopen_app_interrupt() 做 SYNC/PDO 实时处理
 *   - CANopen 任务  = "mainline 线程":canopen_app_process() 做 CO_process 主处理
 */
#include <stdio.h>
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "app.h"
#include "CO_app_STM32.h"

/* CANopen 节点对象:CAN1 @250kbps,节点 ID=1,1ms 定时器用 htim7 */
static CANopenNodeSTM32 appCanopenNode;

static void vCanopenTask(void *pvParameters);

/* 应用层初始化:配置 CANopen 节点 + 创建任务 */
void APP_Init(void)
{
    appCanopenNode.CANHandle      = &hcan1;
    appCanopenNode.HWInitFunction = MX_CAN1_Init;
    appCanopenNode.timerHandle    = &htim7;
    appCanopenNode.desiredNodeID  = 1;
    appCanopenNode.baudrate       = 250; /* kbps,与 MX_CAN1_Init 一致 */

    /* CANopen 主任务:优先级最高(实时线程是 TIM7 ISR,优先级 2) */
    xTaskCreate(vCanopenTask, "CANopen", 512, NULL, 3, NULL);
}

/* FreeRTOS tick 钩子:保持 HAL_GetTick() 前进。
 * FreeRTOS 占用了 SysTick,否则 uwTick 永远为 0,HAL_GetTick() 停摆。 */
void vApplicationTickHook(void)
{
    HAL_IncTick();
}

/* TIM7 1ms 周期中断回调:喂给 CANopen 栈的 1ms 实时任务。
 * 该中断优先级(2)高于 FreeRTOS 内核,ISR 内禁止调用 FreeRTOS API。 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == &htim7) {
        if (appCanopenNode.canOpenStack != NULL) {
            canopen_app_interrupt();
        }
    }
}

/* CANopen 主任务:初始化栈,然后以 1ms 周期跑主处理逻辑。
 * LED 状态取自 CANopen 栈:本板 PF10=绿灯(运行),PF9=红灯(错误)。 */
static void vCanopenTask(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime;

    canopen_app_init(&appCanopenNode);

    xLastWakeTime = xTaskGetTickCount();
    while (1) {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1));

        canopen_app_process();
        
                /* 板载 LED 低电平点亮:状态为 1(亮)时引脚输出低。
         * 本板灯位:PF9=红灯,PF10=绿灯(注意与模板注释相反)。 */
        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10,
                          appCanopenNode.outStatusLEDGreen ? GPIO_PIN_RESET : GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9,
                          appCanopenNode.outStatusLEDRed ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }
}
