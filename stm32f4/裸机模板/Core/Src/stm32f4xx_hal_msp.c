/**
 * @file    stm32f4xx_hal_msp.c
 * @brief   HAL MSP 回调:负责引脚、时钟、NVIC 等板级接线
 */

#include "stm32f4xx_hal.h"
#include "main.h"

/* TIM7 时钟与中断 */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM7) {
        return;
    }

    __HAL_RCC_TIM7_CLK_ENABLE();

    HAL_NVIC_SetPriority(TIM7_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(TIM7_IRQn);
}
