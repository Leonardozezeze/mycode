/**
 * @file    app.c
 * @brief   应用层:裸机超级循环逻辑 + 定时器回调
 */

#include "main.h"
#include "app.h"
#include <stdio.h>

/* 应用层初始化:在 BSP_Init 之后调用,BSP_Init 已初始化 USART1,printf 可用 */
void APP_Init(void)
{
    printf("\r\n[APP] startup @ %lu Hz\r\n", (unsigned long)SystemCoreClock);
    uint8_t send_data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    CAN_SendMessage(&hcan1, 0x123, send_data, 8, 0);
}

/* 应用层主逻辑:超级循环每轮调用一次。
 * 标志任务:PC13 LED 每 500ms 翻转一次(低电平点亮),并打印节拍。
 * CAN 测试:每轮(500ms)发一帧 0x123,data[0] 为递增计数,供上位机观察。 */
static uint32_t s_tick = 0;

void APP_Loop(void)
{
    HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);
    printf("tick %lu\r\n", (unsigned long)++s_tick);
    delay_ms(500);

    /* 周期发送测试帧:CAN ID 0x123,标准帧,8字节,data[0]=计数 */
    static uint8_t tx_cnt = 0;
    uint8_t send_data[8] = {tx_cnt++, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    CAN_SendMessage(&hcan1, 0x123, send_data, 8, 0);
}

/* TIM4 1ms 周期中断回调:硬件定时器 ISR 示例 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == &htim4) {
        /* 可在此放置 1ms 周期任务,如软件节拍累加 */
    }
}
