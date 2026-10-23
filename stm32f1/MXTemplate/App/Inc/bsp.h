/**
 * @file    bsp.h
 * @brief   板级支持:句柄集中声明 + 统一初始化入口
 */

#ifndef __BSP_H
#define __BSP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdio.h>
#include <stdint.h>

/* =============================================================================
 * 板载硬件宏
 * ============================================================================= */

/* 板载 LED:PC13(低电平点亮,Blue Pill) */
#define LED_GPIO_PORT   GPIOC
#define LED_GPIO_PIN    GPIO_PIN_13

/* 调试串口:USART1 PA9(TX)/PA10(RX), 115200-8-N-1 */
#define USART1_GPIO_PORT   GPIOA
#define USART1_TX_PIN      GPIO_PIN_9
#define USART1_RX_PIN      GPIO_PIN_10

/* =============================================================================
 * UART 句柄(在 main.c 中定义)
 * ============================================================================= */
extern UART_HandleTypeDef huart1;  /* USART1, 调试串口(printf 重定向目标) */
extern UART_HandleTypeDef huart2;  /* USART2, 命令输出(对接 F4) */

/* =============================================================================
 * 板级 API
 * ============================================================================= */

/* 板级初始化:一次调用完成时钟 + 全部外设初始化 */
void BSP_Init(void);

/* 延时:基于 DWT 周期计数,由 BSP_Init 自动调用 delay_init() 使能 */
void delay_init(void);
void delay_us(uint32_t us);
void delay_ms(uint32_t ms);

/* printf 重定向(供 stdio 调用) */
int __io_putchar(int ch);

/* UART2 格式化输出(供命令模块使用,最大 128 字节) */
void UART2_SendFormatted(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

/* =============================================================================
 * C++ 专用声明(RingBuffer 实例)
 * 注:ringbuf.h 自带 #ifdef __cplusplus 守卫,C 文件 include 时不会出错
 * ============================================================================= */
#ifdef __cplusplus
#include "ringbuf.h"
extern RingBuffer g_rx;  /* USART1 接收环形缓冲区(在 bsp.cpp 中定义) */
#endif

#endif /* __BSP_H */
