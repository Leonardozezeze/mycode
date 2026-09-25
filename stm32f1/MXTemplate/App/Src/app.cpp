#include "app.h"


/* ========== 私有变量 ========== */
extern ADC_HandleTypeDef hadc1;

/* ========== 初始化 ========== */
void App_Init(void)
{
    BSP_Init();
    printf("App init done\r\n");
}

/* ========== 主循环 ========== */
void App_Loop(void)
{
    HAL_GPIO_TogglePin(LED_GPIO_PORT,LED_GPIO_PIN);
    delay_ms(1000);
}
