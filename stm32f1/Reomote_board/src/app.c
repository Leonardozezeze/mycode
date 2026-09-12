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
}

// 应用层主逻辑:超级循环每轮调用一次。

void APP_Loop(void)
{
    HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);
    printf("x:%d\n", ADC_GetRaw(0));
    printf("y:%d\n\n", ADC_GetRaw(1));
    delay_ms(1100);
}

