/**
 * @file    bsp.c
 * @brief   板级支持:时钟 + 外设初始化(统一入口 BSP_Init)
 */

#include "bsp.h"

/* 句柄定义 */
TIM_HandleTypeDef htim7;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM7_Init(void);

/* 板级初始化:统一入口,按顺序初始化时钟和所有外设 */
void BSP_Init(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM7_Init();
    delay_init();   /* 使能 DWT 周期计数,驱动 delay_us/delay_ms */
}

/* 系统时钟:HSE 8MHz -> PLL(8/336/2/4) -> SYSCLK 168MHz,
 * HCLK=168MHz, PCLK1=42MHz(APB1), PCLK2=84MHz */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 8;
    RCC_OscInitStruct.PLL.PLLN       = 336;
    RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ       = 4;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }
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
/* 阻塞延时 us 微秒(单次上限约 25 秒@168MHz) */
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
/* GPIO:板载 LED*/
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin   = LED_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);
}

/* TIM7 基础定时器,1ms 周期中断:定时器时钟 2*PCLK1=84MHz,
 * Prescaler=83 -> 1MHz, Period=999 -> 1ms */
static void MX_TIM7_Init(void)
{
    htim7.Instance               = TIM7;
    htim7.Init.Prescaler         = 83;
    htim7.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim7.Init.Period            = 999;
    htim7.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim7) != HAL_OK) {
        Error_Handler();
    }
    HAL_TIM_Base_Start_IT(&htim7);   /* 启动 1ms 周期中断,驱动 HAL_TIM_PeriodElapsedCallback */
}
