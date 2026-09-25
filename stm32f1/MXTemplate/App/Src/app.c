#include "app.h"

/* ========== 私有变量 ========== */

/* ========== 初始化 ========== */
void App_Init(void)
{
    BSP_Init();
    // HAL_ADCEx_Calibration_Start(&hadc1);
    printf("App init done\r\n");
}

/* ========== 主循环 ========== */
void App_Loop(void)
{
    printf("loop...\r\n");
    HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);
    // HAL_ADC_Start(&hadc1);                        // 触发新一轮转换
    // HAL_ADC_PollForConversion(&hadc1, 100);
    // volatile uint16_t adcv=HAL_ADC_GetValue(&hadc1);
    // printf("adc:%d\r\n",adcv );
    delay_ms(600);
}
