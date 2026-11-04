#include "encoder.h"
#include "stm32f4xx.h"

/* 根据 CubeMX 配置修改：TIM4_CH1=PD12, TIM4_CH2=PD13, TI12 四倍频 */
extern TIM_HandleTypeDef htim4;
#define ENCODER_TIM (&htim4)

void encoder_init(void)
{
    /* HAL_TIM_Encoder_Start 才会使能计数器；只调用 MX_TIM4_Init 时 CNT 不动 */
    (void)HAL_TIM_Encoder_Start(ENCODER_TIM, TIM_CHANNEL_ALL);
    __HAL_TIM_SET_COUNTER(ENCODER_TIM, 0);
}

int16_t encoder_get_count(void)
{
    /* 硬件为 16 位计数器（ARR=65535），强转 int16_t 得到相对 0 的有符号值 */
    int16_t count = (int16_t)__HAL_TIM_GET_COUNTER(ENCODER_TIM);
    return (int16_t)(count * ENCODER_DIR);
}

void encoder_reset(void)
{
    __HAL_TIM_SET_COUNTER(ENCODER_TIM, 0);
}
