/**
 * @file    app.c
 * @brief   应用层:裸机超级循环逻辑 + 定时器回调
 */

#include "main.h"
#include "app.h"

/* 应用层初始化:无资源需要预先创建 */
void APP_Init(void)
{
    
}

/* 应用层主逻辑:超级循环每轮调用一次。
 * 标志任务:PC13 LED 每 500ms 翻转一次(低电平点亮)。 */
void APP_Loop(void)
{
    HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);
    delay_ms(500);
}

/* TIM4 1ms 周期中断回调:硬件定时器 ISR 示例 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == &htim4) {
        /* 可在此放置 1ms 周期任务,如软件节拍累加 */
    }
}
