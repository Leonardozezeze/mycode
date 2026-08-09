/**
 * @file    app.c
 * @brief   应用层:裸机超级循环逻辑 + 定时器回调
 */

#include "main.h"
#include "app.h"
#include <stdio.h>
#include "CO_app_STM32.h"

/* CANopen 节点对象:CAN1 @250kbps,节点 ID=1,1ms 定时器用 htim7 */
static CANopenNodeSTM32 appCanopenNode;

void APP_Init(void)
{
    appCanopenNode.CANHandle      = &hcan1;
    appCanopenNode.HWInitFunction = MX_CAN1_Init;
    appCanopenNode.timerHandle    = &htim7;
    appCanopenNode.desiredNodeID  = 1;
    appCanopenNode.baudrate       = 250; /* kbps,与 MX_CAN1_Init 一致 */
    canopen_app_init(&appCanopenNode);
}

/* 应用层主逻辑:超级循环每轮调用一次。
 * LED 状态取自 CANopen 栈(PF9 绿=运行,PF10 红=错误),同参考例程。 */
void APP_Loop(void)
{
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);
    printf("USART1 printf OK: baud=115200, 8N1\r\n");
    canopen_app_process();
}

/* TIM7 1ms 周期中断回调:喂给 CANopen 栈的 1ms 定时任务 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == &htim7) {
        if (appCanopenNode.canOpenStack != NULL) {
            canopen_app_interrupt();
        }
    }
}
