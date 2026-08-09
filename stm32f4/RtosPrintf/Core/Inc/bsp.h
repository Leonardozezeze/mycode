/**
 * @file    bsp.h
 * @brief   板级支持:句柄集中声明 + 统一初始化入口
 */

#ifndef __BSP_H
#define __BSP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* 句柄集中声明 */
extern TIM_HandleTypeDef htim7;   /* TIM7, 1ms 中断定时器 */
extern UART_HandleTypeDef huart1; /* USART1, 115200-8-N-1, PA9/PA10 */

/* 板级初始化:一次调用完成时钟 + 全部外设初始化 */
void BSP_Init(void);

/* 全局错误处理(定义在 main.c) */
void Error_Handler(void);

void uart_putc(uint8_t ch);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_H */
