/**
 * @file    bsp.h
 * @brief   板级支持:句柄集中声明 + 统一初始化入口
 */

#ifndef __BSP_H
#define __BSP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

/* 板载 LED:PC13(低电平点亮,如 Blue Pill) */
#define LED_GPIO_PORT   GPIOC
#define LED_GPIO_PIN    GPIO_PIN_13

/* 调试串口:USART1 PA9(TX)/PA10(RX), 115200-8-N-1(Blue Pill 板载 USB-TTL 默认串口) */
#define USART1_GPIO_PORT   GPIOA
#define USART1_TX_PIN      GPIO_PIN_9
#define USART1_RX_PIN      GPIO_PIN_10

/* 句柄集中声明 */
extern TIM_HandleTypeDef htim4;    /* TIM4, 1ms 中断定时器(F103xB 无 TIM6/7,以 TIM4 替代) */
extern UART_HandleTypeDef huart1;  /* USART1, 调试串口(printf 重定向目标) */

/* 板级初始化:一次调用完成时钟 + 全部外设初始化 */
void BSP_Init(void);

/* 全局错误处理(定义在 main.c) */
void Error_Handler(void);

/* 延时:基于 DWT 周期计数,由 BSP_Init 自动调用 delay_init() 使能 */
void delay_init(void);
void delay_us(uint32_t us);
void delay_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_H */
