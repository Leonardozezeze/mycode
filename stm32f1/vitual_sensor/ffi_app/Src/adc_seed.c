#include "adc_seed.h"
uint64_t Get_ADC_Seed64(void)
{
    uint64_t seed = 0;
    for (int i = 0; i < 16; i++)
    {
        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
        {
            seed ^= (uint64_t)(HAL_ADC_GetValue(&hadc1) & 0x0F) << (i * 4);
        }
        HAL_ADC_Stop(&hadc1);
    }
    return seed;
}