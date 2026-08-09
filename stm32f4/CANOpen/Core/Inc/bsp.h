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
extern UART_HandleTypeDef huart1; /* USART1, 调试串口(printf 重定向目标) */
extern CAN_HandleTypeDef hcan1;   /* CAN1, CANopen 总线 */

/* 板级初始化:一次调用完成时钟 + 全部外设初始化 */
void BSP_Init(void);

/* CAN1 初始化(CANopenNodeSTM32.HWInitFunction 需要引用) */
void MX_CAN1_Init(void);

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
