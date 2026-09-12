#ifndef __ADC_H
#define __ADC_H

#include "stm32f1xx_hal.h"

/* ========== 可配置部分 ========== */
#define ADC_CHANNEL_NUM 2 /* 采集通道数 */
#define ADC_VREF 3.3f
#define ADC_MAX_VALUE 4095.0f
#define ADC_FILTER_TIMES 8 /* 滤波平均次数 */

/* ========== 外部变量 ========== */
extern ADC_HandleTypeDef hadc1;
extern DMA_HandleTypeDef hdma_adc1;
extern uint16_t adc_dma_buf[ADC_CHANNEL_NUM]; /* 各通道最新原始值 */

/* ========== 接口 ========== */
void ADC1_Init(void);
void ADC1_Start(void);
uint16_t ADC_GetRaw(uint8_t index);
float ADC_GetVoltage(uint8_t index);
float ADC_GetVoltage_Filtered(uint8_t index);

#endif