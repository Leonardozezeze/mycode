#include "app.h"

/* ========== 初始化 ========== */
void App_Init(void)
{
    BSP_Init();
    printf("App init done\r\n");
}

/* ========== 主循环 ========== */
void App_Loop(void)
{
    HAL_GPIO_WritePin(RED_LED_PORT, RED_LED_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GREEN_LED_PORT, GREEN_LED_PIN, GPIO_PIN_SET);
    delay_ms(500);
    printf("red light!\r\n");
    HAL_GPIO_WritePin(RED_LED_PORT, RED_LED_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GREEN_LED_PORT, GREEN_LED_PIN, GPIO_PIN_RESET);
    delay_ms(500);
    printf("green light!\r\n");
}
