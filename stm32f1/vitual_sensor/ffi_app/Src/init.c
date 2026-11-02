//这里放底层外设的初始化 封装成函数给rust调用
#include "led.h"
#include "delay.h"
#include "uart.h"
#include "adc_seed.h"
void board_drive_init(void)
{
    HAL_ADCEx_Calibration_Start(&hadc1);   // 校准adc
    delay_init();
    uart1_rx_start();
}
