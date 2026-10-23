/**
 * @file    bsp.cpp
 * @brief   板级支持:时钟 + 外设初始化(统一入口 BSP_Init)
 */

#include "bsp.h"
#include "ringbuf.h"
#include "shell.h"

// UART 接收字节(中断用)
uint8_t rx_byte_uart1;
uint8_t rx_byte_uart4;

// 环形缓冲区实例(固定 256 字节,由 RINGBUF_SIZE 宏控制)
RingBuffer g_rx_uart1;
RingBuffer g_rx_uart4;

// 输出函数前向声明
static void uart1_puts(const char* s);
static void uart4_puts(const char* s);

// Shell 实例(静态分配)
Shell g_shell_uart1("> ", uart1_puts, &g_rx_uart1);
Shell g_shell_uart4("$ ", uart4_puts, &g_rx_uart4);

/* 板级初始化 */
void BSP_Init(void)
{
    delay_init();

    // 初始化环形缓冲区(构造函数已自动初始化,这里保留用于兼容)
    g_rx_uart1.init();
    g_rx_uart4.init();

    // Shell 实例已在全局区静态构造

    // 武装 UART 中断接收
    HAL_UART_Receive_IT(&huart1, &rx_byte_uart1, 1);
    HAL_UART_Receive_IT(&huart4, &rx_byte_uart4, 1);
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

/* ---------- DWT 周期计数助手 ----------
 * 只读消费 CYCCNT,与 delay_us 的忙等互不干扰。
 * 传"差值"而非绝对值:无符号减法自动处理 32 位回绕(25.6s @168MHz)。 */
uint32_t bsp_cycle_count(void)
{
    return DWT->CYCCNT;
}

uint32_t bsp_cycles_to_us(uint32_t cycles)
{
    return cycles / s_ticks_per_us;
}

// UART1 输出函数(用于 printf 和 Shell)
int __io_putchar(int ch)
{
    uint8_t c = (uint8_t)ch;
    HAL_UART_Transmit(&huart1, &c, 1, HAL_MAX_DELAY);
    return ch;
}

static void uart1_puts(const char* s)
{
    while (*s) {
        __io_putchar(*s++);
    }
}

// UART4 输出函数(用于第二个 Shell)
static void uart4_puts(const char* s)
{
    while (*s) {
        uint8_t c = (uint8_t)*s++;
        HAL_UART_Transmit(&huart4, &c, 1, HAL_MAX_DELAY);
    }
}

// UART 接收完成回调
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        g_rx_uart1.push(rx_byte_uart1);           // 塞入缓冲区
        HAL_UART_Receive_IT(&huart1, &rx_byte_uart1, 1); // 重新武装
    }
    else if (huart->Instance == UART4) {
        g_rx_uart4.push(rx_byte_uart4);           // 塞入缓冲区
        HAL_UART_Receive_IT(&huart4, &rx_byte_uart4, 1); // 重新武装
    }
}
