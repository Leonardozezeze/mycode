/**
 * @file    encoder.c
 * @brief   编码器测速:测频法(M法) + 测周法(T法) + 一阶低通滤波
 *
 * 测频法(M法):固定时间窗口内计数脉冲数,适合高速
 *   - 每 SAMPLE_PERIOD_MS 采样一次,计算窗口内脉冲增量
 *   - 转速 = delta_counts / CPR / SAMPLE_PERIOD * 60
 *
 * 测周法(T法):测量固定脉冲数所用时间,适合低速
 *   - 记录每个脉冲的时间戳,计算相邻脉冲的时间差
 *   - 转速 = 1 / CPR / delta_time * 60
 *   - 静止超时自动清零
 *
 * 滤波:一阶 IIR 低通 y[n] = α·x[n] + (1-α)·y[n-1]
 *   - 减少量化噪声和测量抖动
 *   - α 越小滤波越强,但响应越慢
 */
#include "encoder.h"
#include "bsp.h"
#include <stdio.h>

/* 根据 CubeMX 配置修改 */
extern TIM_HandleTypeDef htim4;
#define ENCODER_TIM (&htim4)

/* ========================= 测频法(M法)状态 ========================= */
static uint16_t s_m_last_raw = 0;        /* 上次采样时的 CNT(16位) */
static uint32_t s_m_last_time_cyc = 0;   /* 上次采样的时刻(DWT周期) */
static volatile float s_m_rpm_raw = 0.0f;    /* 测频法原始转速(滤波前) */
static volatile int32_t s_m_last_delta = 0;     /* 调试:上次脉冲增量 */
static volatile uint32_t s_m_last_elapsed_us = 0; /* 调试:上次经过时间 */

/* ========================= 测周法(T法)状态 ========================= */
static uint32_t s_t_last_pulse_cyc = 0;  /* 最近一次脉冲的时刻(DWT周期) */
static uint16_t s_t_last_raw = 0;        /* 上一次检测到脉冲时的 CNT */
static volatile float s_t_rpm_raw = 0.0f;    /* 测周法原始转速(滤波前) */

/* ========================= 滤波状态 ========================= */
static float s_filter_alpha = ENCODER_FILTER_ALPHA;  /* 滤波系数(可运行时调整) */
static volatile float s_rpm_filtered = 0.0f;         /* 滤波后的转速 */

/* =====================================================================
 *                           初始化
 * ===================================================================== */
void Encoder_Init(void)
{
    /* 启动编码器模式(TIM4 的通道1和通道2) */
    HAL_TIM_Encoder_Start(ENCODER_TIM, TIM_CHANNEL_ALL);
    __HAL_TIM_SET_COUNTER(ENCODER_TIM, 0);

    /* 临界区:防止与 TIM6 ISR 交错 */
    uint32_t pm = __get_PRIMASK();
    __disable_irq();

    /* 测频法状态 */
    uint32_t now_cyc_m = bsp_cycle_count();
    s_m_last_raw = (uint16_t)__HAL_TIM_GET_COUNTER(ENCODER_TIM);
    s_m_last_time_cyc = now_cyc_m;
    s_m_rpm_raw = 0.0f;

    /* 测周法状态 */
    uint32_t now_cyc = bsp_cycle_count();
    s_t_last_pulse_cyc = now_cyc;
    s_t_last_raw = (uint16_t)__HAL_TIM_GET_COUNTER(ENCODER_TIM);
    s_t_rpm_raw = 0.0f;

    /* 滤波状态 */
    s_filter_alpha = ENCODER_FILTER_ALPHA;
    s_rpm_filtered = 0.0f;

    __set_PRIMASK(pm);
}

/* =====================================================================
 *                    一阶低通滤波:IIR
 *
 * 公式: y[n] = α·x[n] + (1-α)·y[n-1]
 *   - α=1: 无滤波,直接输出
 *   - α=0.1: 强滤波,响应慢但平滑
 *   - α=0.3: 折中(推荐)
 * ===================================================================== */
static inline float apply_filter(float raw)
{
    s_rpm_filtered = s_filter_alpha * raw + (1.0f - s_filter_alpha) * s_rpm_filtered;
    return s_rpm_filtered;
}

/* =====================================================================
 *                    测频法(M法):Encoder_Update_M()
 *
 * 原理:固定时间窗口内计数脉冲数
 *   - 计算窗口内脉冲增量 delta_counts
 *   - 用 DWT 测量实际经过的时间 elapsed_us
 *   - 转速 = delta_counts / CPR / elapsed_time * 60
 *
 * 适用:高速(脉冲频率高,固定时间内有足够计数)
 * 优点:实时性好,响应快
 * 缺点:低速时量化误差大(可能只有0或1个脉冲)
 * ===================================================================== */
void Encoder_Update_M(void)
{
    /* 读取当前时刻和 16 位计数器值 */
    uint32_t now_cyc = bsp_cycle_count();
    uint16_t now_raw = (uint16_t)__HAL_TIM_GET_COUNTER(ENCODER_TIM);

    /* 计算脉冲增量(16位回绕安全,使用有符号16位减法处理回绕) */
    int32_t delta = (int16_t)(now_raw - s_m_last_raw);
    s_m_last_raw = now_raw;

    /* 计算实际经过的时间(微秒) */
    uint32_t elapsed_us = bsp_cycles_to_us(now_cyc - s_m_last_time_cyc);
    s_m_last_time_cyc = now_cyc;

    /* 保存调试信息 */
    s_m_last_delta = delta;
    s_m_last_elapsed_us = elapsed_us;

    /* 除零保护 */
    if (elapsed_us == 0)
        elapsed_us = 1;

    /* 测频法公式:
     * RPM = (delta_counts / counts_per_rev) * (60 / elapsed_time_s)
     *     = delta_counts * 60 * 1000000 / (CPR * elapsed_time_us) */
    float rpm_raw = (float)delta * 60000000.0f /
                    (ENCODER_COUNTS_PER_REV * (float)elapsed_us);

    s_m_rpm_raw = (float)ENCODER_SIGN * rpm_raw;

    /* 应用滤波 */
    apply_filter(s_m_rpm_raw);
}

/* =====================================================================
 *                    测周法(T法):Encoder_Update_T()
 *
 * 原理:测量相邻脉冲的时间间隔
 *   - 检测到脉冲时,记录当前时刻
 *   - 计算与上一个脉冲的时间差 delta_t
 *   - 转速 = 1 / CPR / delta_t * 60
 *
 * 适用:低速(脉冲间隔长,时间测量精度高)
 * 优点:低速精度高,分辨率好
 * 缺点:高速时更新时间长(脉冲太密,计算频繁),响应慢
 * ===================================================================== */
void Encoder_Update_T(void)
{
    uint32_t now_cyc = bsp_cycle_count();
    uint16_t now_raw = (uint16_t)__HAL_TIM_GET_COUNTER(ENCODER_TIM);

    /* 检测是否有新脉冲 */
    int32_t delta = (int16_t)(now_raw - s_t_last_raw);

    if (delta != 0)
    {
        /* 有新脉冲,计算时间差 */
        uint32_t delta_cyc = now_cyc - s_t_last_pulse_cyc;
        s_t_last_pulse_cyc = now_cyc;
        s_t_last_raw = now_raw;

        /* 换算为微秒 */
        uint32_t delta_us = bsp_cycles_to_us(delta_cyc);

        if (delta_us > 0)
        {
            /* 测周法公式:
             * RPM = (1 / counts_per_rev) * (60 / delta_time_s)
             *     = 60 / (CPR * delta_time_s)
             *     = 60 * 1000000 / (CPR * delta_time_us)
             * 注意:delta 是带符号的,表示方向 */
            float rpm_raw = (float)delta * 60000000.0f /
                            (ENCODER_COUNTS_PER_REV * (float)delta_us);

            s_t_rpm_raw = (float)ENCODER_SIGN * rpm_raw;

            /* 应用滤波 */
            apply_filter(s_t_rpm_raw);
        }
    }
    else
    {
        /* 无新脉冲,检查是否静止 */
        uint32_t idle_us = bsp_cycles_to_us(now_cyc - s_t_last_pulse_cyc);
        if (idle_us > ENCODER_STOP_TIMEOUT_MS * 1000u)
        {
            /* 超过超时时间,判为静止 */
            s_t_rpm_raw = 0.0f;
            apply_filter(0.0f);  /* 滤波也趋向 0 */
        }
    }
}

/* =====================================================================
 *                    统一更新接口:Encoder_Update()
 *
 * 使用测频法(M法),100ms 采样周期减少低速量化误差
 * ===================================================================== */
void Encoder_Update(void)
{
    Encoder_Update_M();
}

/* =====================================================================
 *                           读取接口
 * ===================================================================== */

/* 读原始 CNT 寄存器值(16位) */
uint16_t Encoder_GetRawCount(void)
{
    return (uint16_t)__HAL_TIM_GET_COUNTER(ENCODER_TIM);
}

/* 读转速(输出轴 RPM,带符号,滤波后) */
float Encoder_GetSpeedRPM(void)
{
    return s_rpm_filtered;  /* 返回滤波后的值 */
}

/* 读测频法原始转速(滤波前,调试用) */
float Encoder_GetSpeedRPM_M(void)
{
    return s_m_rpm_raw;
}

/* 读测周法原始转速(滤波前,调试用) */
float Encoder_GetSpeedRPM_T(void)
{
    return s_t_rpm_raw;
}

/* =====================================================================
 *                           滤波配置
 * ===================================================================== */

/* 运行时调整滤波系数(0~1,1=无滤波) */
void Encoder_SetFilterAlpha(float alpha)
{
    if (alpha < 0.01f) alpha = 0.01f;  /* 下限保护 */
    if (alpha > 1.0f) alpha = 1.0f;    /* 上限保护 */
    s_filter_alpha = alpha;
}

/* 读当前滤波系数 */
float Encoder_GetFilterAlpha(void)
{
    return s_filter_alpha;
}

/* =====================================================================
 *                           调试接口
 * ===================================================================== */
void Encoder_DebugDump(void)
{
    uint16_t cnt_now = (uint16_t)__HAL_TIM_GET_COUNTER(ENCODER_TIM);
    printf("CNT=%u delta=%ld elapsed=%luus\r\n",
           (unsigned int)cnt_now,
           (long)s_m_last_delta,
           (unsigned long)s_m_last_elapsed_us);
    printf("Raw=%.2f RPM  Filtered=%.2f RPM  Alpha=%.3f\r\n",
           (double)s_m_rpm_raw,
           (double)s_rpm_filtered,
           (double)s_filter_alpha);
}
