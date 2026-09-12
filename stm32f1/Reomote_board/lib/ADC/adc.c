#include "adc.h"

/* ================= 全局变量 ================= */
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

uint16_t adc_dma_buf[ADC_CHANNEL_NUM]; /* DMA 循环写入，下标即通道顺序 */

/* ================= 通道映射表 =================
 * 按顺序排列：DMA 缓冲区下标 0..N-1 就对应下面的通道
 * 想改通道/引脚，只改这三行即可
 */
static GPIO_TypeDef *const adc_gpio_port[ADC_CHANNEL_NUM] = {
    GPIOA, GPIOA};
static const uint16_t adc_gpio_pin[ADC_CHANNEL_NUM] = {
    GPIO_PIN_1, GPIO_PIN_2};
static const uint32_t adc_channel[ADC_CHANNEL_NUM] = {
    ADC_CHANNEL_1, ADC_CHANNEL_2};

/* ================= 函数实现 ================= */

/**
 * @brief  初始化 ADC1 多通道（扫描 + 连续 + DMA 循环）
 */
void ADC1_Init(void)
{
    uint8_t i;

    /* 1. 使能用到的 GPIO 时钟 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* 2. 各通道引脚配成模拟输入 */
    for (i = 0; i < ADC_CHANNEL_NUM; i++)
    {
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = adc_gpio_pin[i];
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(adc_gpio_port[i], &GPIO_InitStruct);
    }

    /* 3. 使能 DMA1 时钟（F1 的 ADC1 固定用 DMA1_Channel1） */
    __HAL_RCC_DMA1_CLK_ENABLE();

    hdma_adc1.Instance = DMA1_Channel1;
    hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE; /* 外设地址固定 */
    hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;     /* 内存地址递增 */
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc1.Init.Mode = DMA_CIRCULAR; /* 循环模式，永不停 */
    hdma_adc1.Init.Priority = DMA_PRIORITY_HIGH;
    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK)
    {
        while (1)
        {
        }
    }

    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);

    /* 4. 使能 ADC1 时钟并配置基本参数 */
    __HAL_RCC_ADC1_CLK_ENABLE();

    hadc1.Instance = ADC1;
    hadc1.Init.ScanConvMode = ENABLE;       /* 多通道必须开扫描 */
    hadc1.Init.ContinuousConvMode = ENABLE; /* 连续转换，DMA 自动搬运 */
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START; /* 软件触发 */
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = ADC_CHANNEL_NUM; /* 规则组转换个数 */
    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        while (1)
        {
        }
    }

    /* 5. 逐个配置规则组通道（Rank 从 1 开始） */
    for (i = 0; i < ADC_CHANNEL_NUM; i++)
    {
        ADC_ChannelConfTypeDef sConfig = {0};
        sConfig.Channel = adc_channel[i];
        sConfig.Rank = i + 1;
        sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
        if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
        {
            while (1)
            {
            }
        }
    }

    /* 6. F1 系列必须执行 ADC 校准 */
    HAL_ADCEx_Calibration_Start(&hadc1);
}

/**
 * @brief  启动 ADC + DMA，之后 adc_dma_buf 会被硬件自动循环刷新
 */
void ADC1_Start(void)
{
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_dma_buf, ADC_CHANNEL_NUM);
}

/**
 * @brief  读取指定通道的原始值
 * @param  index 通道下标（0 ~ ADC_CHANNEL_NUM-1），对应映射表顺序
 * @retval 0 ~ 4095
 */
uint16_t ADC_GetRaw(uint8_t index)
{
    if (index >= ADC_CHANNEL_NUM)
    {
        return 0;
    }
    return adc_dma_buf[index];
}

/**
 * @brief  读取指定通道电压
 * @retval 0 ~ 3.3V
 */
float ADC_GetVoltage(uint8_t index)
{
    return (float)ADC_GetRaw(index) * ADC_VREF / ADC_MAX_VALUE;
}

/**
 * @brief  多次采样取平均后换算电压（在时间上分散取值，避开瞬时抖动）
 */
float ADC_GetVoltage_Filtered(uint8_t index)
{
    uint32_t sum = 0;
    uint8_t i;

    if (index >= ADC_CHANNEL_NUM)
    {
        return 0.0f;
    }

    for (i = 0; i < ADC_FILTER_TIMES; i++)
    {
        sum += adc_dma_buf[index];
        HAL_Delay(1); /* 让 DMA 至少刷新一轮；不需要可去掉 */
    }

    return ((float)sum / (float)ADC_FILTER_TIMES) * ADC_VREF / ADC_MAX_VALUE;
}