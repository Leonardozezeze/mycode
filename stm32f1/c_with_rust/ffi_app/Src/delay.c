#include "stm32f1xx.h"
static volatile uint32_t s_ticks_per_us = 0; /* 每微秒的 CPU 周期数(按 SystemCoreClock 换算) */
/* 使能 DWT->CYCCNT 周期计数器(幂等,由 BSP_Init 调用,可重复调用) */
void delay_init(void)
{
    s_ticks_per_us = SystemCoreClock / 1000000u;

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; /* 使能 DWT 寄存器访问 */
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk; /* 启动周期计数 */
}

/* 阻塞延时 us 微秒(单次上限约 59 秒@72MHz) */
void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * s_ticks_per_us; /* 目标周期数 */
    while ((DWT->CYCCNT - start) < ticks)
    { /* 无符号减法,自动处理 CYCCNT 溢出回绕 */
    }
}

/* 阻塞延时 ms 毫秒:逐毫秒累加,避免单次换算周期数过大 */
void delay_ms(uint32_t ms)
{
    while (ms--)
    {
        delay_us(1000);
    }
}