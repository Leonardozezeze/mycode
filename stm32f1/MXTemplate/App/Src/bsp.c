/**
 * @file    bsp.c
 * @brief   板级支持:时钟 + 外设初始化(统一入口 BSP_Init)
 */

#include "bsp.h"
#include "ringbuf.h"
static ringbuf_t g_rx;
/* 板级初始化 */
void BSP_Init(void)
{
    delay_init();
    ringbuf_init(&g_rx);
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}
/* ==============================================================================
 * 延时:基于 DWT 周期计数器
 * 精度为 1 个 CPU 周期,CYCCNT 在中断期间持续计数,延时为真实墙钟时间。
 * 不占用 SysTick —— SysTick 仍归 HAL 的 HAL_GetTick()/HAL_Delay() 使用。
 * ============================================================================== */
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

int __io_putchar(int ch)
{
    uint8_t c = (uint8_t)ch;
    HAL_UART_Transmit(&huart1, &c, 1, HAL_MAX_DELAY);
    return ch;
}
/* 回调里再启动下一次接收 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        ringbuf_push(&g_rx, rx_byte);              /* 只塞缓冲 */
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1); /* 重新武装 */
    }
}