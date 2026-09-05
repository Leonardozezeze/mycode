/**
  ******************************************************************************
  * @file    Templates/Src/stm32f1xx_hal_msp.c
  * @author  MCD Application Team
  * @brief   HAL MSP module.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2016 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/** @addtogroup STM32F1xx_HAL_Examples
  * @{
  */

/** @addtogroup Templates
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/** @defgroup HAL_MSP_Private_Functions
  * @{
  */

/**
  * @brief  Initializes the Global MSP.
  * @param  None
  * @retval None
  */
void HAL_MspInit(void)
{ 
}

/**
  * @brief  DeInitializes the Global MSP.
  * @param  None  
  * @retval None
  */
void HAL_MspDeInit(void)
{
}

/**
  * @brief  Initializes the TIM_Base MSP(TIM4 clock and NVIC).
  * @param  htim_base: TIM Base handle
  * @retval None
  */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim_base)
{
  if (htim_base->Instance == TIM4)
  {
    __HAL_RCC_TIM4_CLK_ENABLE();
    HAL_NVIC_SetPriority(TIM4_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM4_IRQn);
  }
}

/**
  * @brief  DeInitializes the TIM_Base MSP.
  * @param  htim_base: TIM Base handle
  * @retval None
  */
void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef *htim_base)
{
  if (htim_base->Instance == TIM4)
  {
    __HAL_RCC_TIM4_CLK_DISABLE();
    HAL_NVIC_DisableIRQ(TIM4_IRQn);
  }
}

/**
  * @brief  Initializes the UART MSP (USART1 clock, PA9/PA10 pins, NVIC).
  * @param  huart: UART handle
  * @retval None
  */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if (huart->Instance == USART1)
  {
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA9  -> USART1_TX, PA10 -> USART1_RX */
    GPIO_InitStruct.Pin   = USART1_TX_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(USART1_GPIO_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = USART1_RX_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(USART1_GPIO_PORT, &GPIO_InitStruct);

    /* USART1 全局中断:为后续中断接收预留;阻塞式 printf 不依赖它 */
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
  }
}

/**
  * @brief  DeInitializes the UART MSP.
  * @param  huart: UART handle
  * @retval None
  */
void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    __HAL_RCC_USART1_CLK_DISABLE();
    HAL_GPIO_DeInit(USART1_GPIO_PORT, USART1_TX_PIN | USART1_RX_PIN);
    HAL_NVIC_DisableIRQ(USART1_IRQn);
  }
}

/**
 * @brief CAN 底层硬件初始化 (GPIO, 时钟, 中断)
 * @param hcan CAN句柄指针
 */
void HAL_CAN_MspInit(CAN_HandleTypeDef *hcan)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if (hcan->Instance == CAN1)
  {
    // 1. 使能CAN1和GPIOA的时钟
    __HAL_RCC_CAN1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    // 2. 配置CAN的GPIO: PA12=TX 复用推挽输出, PA11=RX 输入
    GPIO_InitStruct.Pin = GPIO_PIN_12;          /* CAN1_TX */
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;     /* 复用推挽输出 */
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_11;          /* CAN1_RX 是输入!不能配成推挽输出 */
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // // 3. (可选) 配置CAN中断优先级
    // HAL_NVIC_SetPriority(CAN1_RX0_IRQn, 0, 0);
    // HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);
  }
}

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */
