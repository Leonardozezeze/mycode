#ifndef __ADC_SEED_H
#define __ADC_SEED_H
#include <stdint.h>
#include <stm32f1xx.h>
extern ADC_HandleTypeDef hadc1;
uint64_t Get_ADC_Seed64(void);
#endif