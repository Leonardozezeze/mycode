/**
 * @file    bsp.c
 * @brief   板级支持:时钟 + 外设初始化(统一入口 BSP_Init)
 */

#include "bsp.h"

/* 句柄定义 */
TIM_HandleTypeDef htim4;
CAN_HandleTypeDef hcan;
UART_HandleTypeDef huart1;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM4_Init(void);
static void MX_CAN_Init(void);
static void MX_USART1_UART_Init(void);

/* 板级初始化:统一入口,按顺序初始化时钟和所有外设
 * 注意:TIM4 仅做配置,中断启动由 CANopen 协议栈在初始化完成后负责 */
void BSP_Init(void)
{
    HAL_Init();
    SystemClock_Config();
    delay_init(); /* 使能 DWT 周期计数,驱动 delay_us/delay_ms */
    MX_GPIO_Init();
    MX_CAN_Init();
    MX_USART1_UART_Init();
    MX_TIM4_Init();  /* 仅配置定时器,不启动中断 */
}

/* 系统时钟:HSE 8MHz -> PLL(x9) -> SYSCLK 72MHz,
 * HCLK=72MHz, PCLK1=36MHz(APB1), PCLK2=72MHz(APB2),
 * APB1 分频 >1,故其定时器时钟 = 2 x PCLK1 = 72MHz */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL     = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;   /* APB1 上限 36MHz */
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
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
void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
/* GPIO:仅板载 LED PC13(低电平点亮)。
 * 外设引脚由各自 HAL_*_MspInit 配置:USART1 PA9/10 -> HAL_UART_MspInit,
 * CAN1 PA11/12 -> HAL_CAN_MspInit(stm32f1xx_hal_msp.c)。 */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* LED PC13 */
    GPIO_InitStruct.Pin   = LED_GPIO_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_PORT, &GPIO_InitStruct);
}

/* TIM4 通用定时器,1ms 周期中断:定时器时钟 2*PCLK1=72MHz,
 * Prescaler=71 -> 1MHz, Period=999 -> 1ms
 * 仅做配置,中断启动由 CANopen 协议栈在初始化完成后调用 HAL_TIM_Base_Start_IT() */
static void MX_TIM4_Init(void)
{
    htim4.Instance               = TIM4;
    htim4.Init.Prescaler         = 71;
    htim4.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim4.Init.Period            = 999;
    htim4.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim4) != HAL_OK) {
        Error_Handler();
    }
    /* 不在此处启动中断,由 CANopen 协议栈初始化完成后启动 */
}

/* CAN:PA11(CAN_RX), PA12(CAN_TX), 250kbps @ APB1=36MHz
 * 位时序: Prescaler=8, BS1=13tq, BS2=4tq, SJW=1tq, 同步点=77.8%
 * 波特率 = 36MHz / 8 / (1+13+4) = 250kbps */
static void MX_CAN_Init(void)
{
    hcan.Instance = CAN1;
    hcan.Init.Prescaler = 8;
    hcan.Init.Mode = CAN_MODE_NORMAL;
    hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan.Init.TimeSeg1 = CAN_BS1_13TQ;
    hcan.Init.TimeSeg2 = CAN_BS2_4TQ;
    hcan.Init.TimeTriggeredMode = DISABLE;
    hcan.Init.AutoBusOff = ENABLE;
    hcan.Init.AutoWakeUp = DISABLE;
    hcan.Init.AutoRetransmission = ENABLE;
    hcan.Init.ReceiveFifoLocked = DISABLE;
    hcan.Init.TransmitFifoPriority = DISABLE;
    /* 注:引脚(PA11/12)与 NVIC 配置在 HAL_CAN_MspInit 中,由上面的 HAL_CAN_Init 自动回调 */
    if (HAL_CAN_Init(&hcan) != HAL_OK) {
        Error_Handler();
    }
}

/* CAN1 初始化公共入口:供 CANopen 通信复位时调用 */
void BSP_CAN_Init(void)
{
    MX_CAN_Init();
}

/* USART1:PA9(TX), PA10(RX), 115200 8N1
 * USART1 在 APB2 总线上,时钟 = 72MHz
 * BRR = 72MHz / 115200 = 625 = 0x271 */
static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        Error_Handler();
    }
}
