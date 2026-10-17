/**
 * @file    encoder.c
 * @brief   编码器测速:TIM6 中断固定 10ms 采样,内部 DWT 时基,与外部调用节奏无关
 *
 * 原理:实测窗口 M 法 + 低速自适应窗口扩展
 *   - Encoder_Update() 每 10ms(中断)把计数增量累加进"开放窗口";
 *   - 窗口内计数 ≥ ENCODER_MIN_COUNTS_PER_WINDOW,或窗口达到
 *     ENCODER_MAX_WINDOW_MS 上限时关闭窗口,按 DWT 实测的精确
 *     elapsed_us 计算 RPM(窗口/**
 * @file    encoder.c
 * @brief   编码器测速:TIM6 中断固定 10ms 采样,内部 DWT 时基,与外部调用节奏无关
 *
 * 原理:实测窗口 M 法 + 低速自适应窗口扩展
 *   - Encoder_Update() 每 10ms(中断)把计数增量累加进"开放窗口";
 *   - 窗口内计数 ≥ ENCODER_MIN_COUNTS_PER_WINDOW,或窗口达到
 *     ENCODER_MAX_WINDOW_MS 上限时关闭窗口,按 DWT 实测的精确
 *     elapsed_us 计算 RPM(窗口是 10ms 还是 45ms 都用真实时间,
 *     中断抖动不影响精度);
 *   - 关窗时推入 8 点滑动平均,结果缓存到 volatile float,外部随时读;
 *   - DWT 时间戳超过 ENCODER_STOP_TIMEOUT_MS 无脉冲 → 判静止,清零。
 *
 * 并发模型:
 *   - 测速状态(s_rpm_、s_acc_、s_win_ 前缀变量及滤波缓冲)只在 TIM6 ISR
 *     里读写,对外仅暴露 volatile float 缓存(对齐字读,原子);
 *   - 累计计数状态(s_total_count/s_last_raw/s_last_delta)只在主上下文
 *     使用,与测速状态不相交;
 *   - Encoder_Init/Reset 清状态时用 PRIMASK 临界区,防止与 ISR 交错。
 */
#include "encoder.h"
#include "bsp.h"
#include <stdio.h>

/* 根据你的 CubeMX 配置修改 */
extern TIM_HandleTypeDef htim2;
#define ENCODER_TIM (&htim2)

/* ---------- 累计计数状态(仅主上下文) ---------- */
static int32_t s_total_count = 0;
static uint16_t s_last_raw = 0;
static int16_t s_last_delta = 0;

/* ---------- 测速状态(仅 Encoder_Update,即 TIM6 ISR) ---------- */
static uint16_t s_rpm_last_raw = 0;   /* 上次 Update 时的 CNT */
static int32_t s_acc_counts = 0;      /* 当前开放窗口内累计计数(带符号) */
static uint32_t s_win_start_cyc = 0;  /* 窗口起点(DWT 周期快照) */
static uint32_t s_last_pulse_cyc = 0; /* 最近一次非零增量的时刻(DWT 周期) */
static volatile float s_rpm_raw = 0.0f;    /* 最近关窗的转速(未滤波) */
static volatile float s_rpm_cached = 0.0f; /* 滑动平均后的转速(对外值) */

/* ---------- 滑动平均滤波(仅 ISR 写,关窗时推入一次) ---------- */
static float s_rpm_buf[ENCODER_RPM_FILTER_N] = {0};
static uint8_t s_rpm_idx = 0;
static uint8_t s_rpm_cnt = 0;

/* ---------- 调试计数器(证明 ISR 在跑) ---------- */
static volatile uint32_t s_dbg_windows = 0; /* 关窗次数 */
static volatile uint32_t s_dbg_stops = 0;   /* 静止清零次数 */

/* ---------- 初始化 ---------- */
void Encoder_Init(void)
{
    HAL_TIM_Encoder_Start(ENCODER_TIM, TIM_CHANNEL_ALL);
    __HAL_TIM_SET_COUNTER(ENCODER_TIM, 0);

    uint32_t pm = __get_PRIMASK();
    __disable_irq();

    s_total_count = 0;
    s_last_raw = 0;
    s_last_delta = 0;

    /* 测速窗口与当前 CNT/时刻同步 */
    uint32_t now_cyc = bsp_cycle_count();
    s_rpm_last_raw = 0; /* CNT 刚清零 */
    s_acc_counts = 0;
    s_win_start_cyc = now_cyc;
    s_last_pulse_cyc = now_cyc;
    s_rpm_raw = 0.0f;
    s_rpm_cached = 0.0f;
    for (uint8_t i = 0; i < ENCODER_RPM_FILTER_N; i++)
        s_rpm_buf[i] = 0.0f;
    s_rpm_idx = 0;
    s_rpm_cnt = 0;

    __set_PRIMASK(pm);
}

/* ---------- 读原始值 ---------- */
uint16_t Encoder_GetRawCount(void)
{
    return (uint16_t)__HAL_TIM_GET_COUNTER(ENCODER_TIM);
}

/* ---------- 读累计总计数(主上下文) ---------- */
int32_t Encoder_GetTotalCount(void)
{
    uint16_t now = Encoder_GetRawCount();

    /* 用 int16_t 做差,自动处理 16 位回绕
     * 只要两次调用之间变化不超过 ±32767 就正确 */
    int16_t diff = (int16_t)(now - s_last_raw);

    s_total_count += diff;
    s_last_raw = now;
    s_last_delta = diff;

    return s_total_count;
}

/* ---------- 读增量 ---------- */
int16_t Encoder_GetDeltaCount(void)
{
    return s_last_delta;
}

/* ---------- 读方向 ---------- */
int8_t Encoder_GetDirection(void)
{
    if (s_last_delta > 0)
        return 1;
    if (s_last_delta < 0)
        return -1;
    return 0;
}

/* =====================================================================
 *                  ISR 采样:Encoder_Update()(TIM6,10ms)
 * ===================================================================== */
void Encoder_Update(void)
{
    uint32_t now_cyc = bsp_cycle_count();
    uint16_t now_raw = Encoder_GetRawCount();

    /* uint16 差值回绕安全(TIM2 为 32 位计数,截断到低 16 位;
     * 10ms 内 ±32767 计数 ≈ ±209800 RPM 上限,实际远达不到) */
    int16_t diff = (int16_t)(uint16_t)(now_raw - s_rpm_last_raw);
    s_rpm_last_raw = now_raw;

    s_acc_counts += diff;
    if (diff != 0)
        s_last_pulse_cyc = now_cyc;

    /* 静止判定:超过 STOP_TIMEOUT 无任何脉冲 → 输出清零、窗口重同步 */
    if (bsp_cycles_to_us(now_cyc - s_last_pulse_cyc) > ENCODER_STOP_TIMEOUT_MS * 1000u)
    {
        s_acc_counts = 0;
        s_win_start_cyc = now_cyc;
        s_rpm_raw = 0.0f;
        s_rpm_cached = 0.0f;
        for (uint8_t i = 0; i < ENCODER_RPM_FILTER_N; i++)
            s_rpm_buf[i] = 0.0f;
        s_rpm_idx = 0;
        s_rpm_cnt = 0;
        s_dbg_stops++;
        return;
    }

    /* 窗口实测时长(DWT 精确计时,与中断抖动无关) */
    uint32_t elapsed_us = bsp_cycles_to_us(now_cyc - s_win_start_cyc);
    if (elapsed_us == 0)
        elapsed_us = 1; /* 除零保护(理论上首个窗口不会关这么早) */

    int32_t abs_acc = (s_acc_counts < 0) ? -s_acc_counts : s_acc_counts;

    /* 关窗条件:计数够了(高速→每 10ms 关一次)或窗口到上限(低速) */
    if (abs_acc >= ENCODER_MIN_COUNTS_PER_WINDOW ||
        elapsed_us >= ENCODER_MAX_WINDOW_MS * 1000u)
    {
        /* RPM = SIGN * counts / CPR * (60e6 / elapsed_us) */
        float rpm = (float)(ENCODER_SIGN * s_acc_counts) * 60000000.0f /
                    (ENCODER_COUNTS_PER_REV * (float)elapsed_us);

        s_rpm_raw = rpm;

        /* 滑动平均:只在关窗时推入一次(窗口延长期间不重复推) */
        s_rpm_buf[s_rpm_idx] = rpm;
        s_rpm_idx = (uint8_t)((s_rpm_idx + 1) % ENCODER_RPM_FILTER_N);
        if (s_rpm_cnt < ENCODER_RPM_FILTER_N)
            s_rpm_cnt++;

        float sum = 0.0f;
        for (uint8_t i = 0; i < s_rpm_cnt; i++)
            sum += s_rpm_buf[i];
        s_rpm_cached = sum / (float)s_rpm_cnt;

        /* 重开窗口 */
        s_acc_counts = 0;
        s_win_start_cyc = now_cyc;
        s_dbg_windows++;
    }
    /* 否则继续累加 —— 低速窗口扩展 */
}

/* ---------- 调试:打印内部状态(仅主上下文调用) ---------- */
void Encoder_DebugDump(void)
{
    uint32_t now_cyc = bsp_cycle_count();
    printf("CNT=%u acc=%ld win=%lums pulse=%lums ago\r\n",
           (unsigned)Encoder_GetRawCount(),
           (long)s_acc_counts,
           (unsigned long)(bsp_cycles_to_us(now_cyc - s_win_start_cyc) / 1000u),
           (unsigned long)(bsp_cycles_to_us(now_cyc - s_last_pulse_cyc) / 1000u));
    printf("raw=%.2f cached=%.2f flt=%u/%u win#=%lu stop#=%lu\r\n",
           (double)s_rpm_raw, (double)s_rpm_cached,
           (unsigned)s_rpm_cnt, (unsigned)ENCODER_RPM_FILTER_N,
           (unsigned long)s_dbg_windows, (unsigned long)s_dbg_stops);
}

/* ---------- 读转速缓存(任何上下文) ---------- */
float Encoder_GetSpeedRPM(void)
{
    return s_rpm_cached;
}

float Encoder_GetSpeedRPM_Raw(void)
{
    return s_rpm_raw;
}

/* ---------- 重置(可从主上下文调用,临界区防 ISR 交错) ---------- */
void Encoder_Reset(void)
{
    uint32_t pm = __get_PRIMASK();
    __disable_irq();

    __HAL_TIM_SET_COUNTER(ENCODER_TIM, 0);

    s_total_count = 0;
    s_last_raw = 0;
    s_last_delta = 0;

    uint32_t now_cyc = bsp_cycle_count();
    s_rpm_last_raw = 0;
    s_acc_counts = 0;
    s_win_start_cyc = now_cyc;
    s_last_pulse_cyc = now_cyc;
    s_rpm_raw = 0.0f;
    s_rpm_cached = 0.0f;
    for (uint8_t i = 0; i < ENCODER_RPM_FILTER_N; i++)
        s_rpm_buf[i] = 0.0f;
    s_rpm_idx = 0;
    s_rpm_cnt = 0;

    __set_PRIMASK(pm);
}

/* ---------- 设置累计值 ---------- */
void Encoder_SetTotalCount(int32_t count)
{
    s_total_count = count;
}
是 10ms 还是 45ms 都用真实时间,
 *     中断抖动不影响精度);
 *   - 关窗时推入 8 点滑动平均,结果缓存到 volatile float,外部随时读;
 *   - DWT 时间戳超过 ENCODER_STOP_TIMEOUT_MS 无脉冲 → 判静止,清零。
 *
 * 并发模型:
 *   - 测速状态(s_rpm_、s_acc_、s_win_ 前缀变量及滤波缓冲)只在 TIM6 ISR
 *     里读写,对外仅暴露 volatile float 缓存(对齐字读,原子);
 *   - 累计计数状态(s_total_count/s_last_raw/s_last_delta)只在主上下文
 *     使用,与测速状态不相交;
 *   - Encoder_Init/Reset 清状态时用 PRIMASK 临界区,防止与 ISR 交错。
 */
#include "encoder.h"
#include "bsp.h"
#include <stdio.h>

/* 根据你的 CubeMX 配置修改 */
extern TIM_HandleTypeDef htim2;
#define ENCODER_TIM (&htim2)

/* ---------- 累计计数状态(仅主上下文) ---------- */
static int32_t s_total_count = 0;
static uint16_t s_last_raw = 0;
static int16_t s_last_delta = 0;

/* ---------- 测速状态(仅 Encoder_Update,即 TIM6 ISR) ---------- */
static uint16_t s_rpm_last_raw = 0;   /* 上次 Update 时的 CNT */
static int32_t s_acc_counts = 0;      /* 当前开放窗口内累计计数(带符号) */
static uint32_t s_win_start_cyc = 0;  /* 窗口起点(DWT 周期快照) */
static uint32_t s_last_pulse_cyc = 0; /* 最近一次非零增量的时刻(DWT 周期) */
static volatile float s_rpm_raw = 0.0f;    /* 最近关窗的转速(未滤波) */
static volatile float s_rpm_cached = 0.0f; /* 滑动平均后的转速(对外值) */

/* ---------- 滑动平均滤波(仅 ISR 写,关窗时推入一次) ---------- */
static float s_rpm_buf[ENCODER_RPM_FILTER_N] = {0};
static uint8_t s_rpm_idx = 0;
static uint8_t s_rpm_cnt = 0;

/* ---------- 调试计数器(证明 ISR 在跑) ---------- */
static volatile uint32_t s_dbg_windows = 0; /* 关窗次数 */
static volatile uint32_t s_dbg_stops = 0;   /* 静止清零次数 */

/* ---------- 初始化 ---------- */
void Encoder_Init(void)
{
    HAL_TIM_Encoder_Start(ENCODER_TIM, TIM_CHANNEL_ALL);
    __HAL_TIM_SET_COUNTER(ENCODER_TIM, 0);

    uint32_t pm = __get_PRIMASK();
    __disable_irq();

    s_total_count = 0;
    s_last_raw = 0;
    s_last_delta = 0;

    /* 测速窗口与当前 CNT/时刻同步 */
    uint32_t now_cyc = bsp_cycle_count();
    s_rpm_last_raw = 0; /* CNT 刚清零 */
    s_acc_counts = 0;
    s_win_start_cyc = now_cyc;
    s_last_pulse_cyc = now_cyc;
    s_rpm_raw = 0.0f;
    s_rpm_cached = 0.0f;
    for (uint8_t i = 0; i < ENCODER_RPM_FILTER_N; i++)
        s_rpm_buf[i] = 0.0f;
    s_rpm_idx = 0;
    s_rpm_cnt = 0;

    __set_PRIMASK(pm);
}

/* ---------- 读原始值 ---------- */
uint16_t Encoder_GetRawCount(void)
{
    return (uint16_t)__HAL_TIM_GET_COUNTER(ENCODER_TIM);
}

/* ---------- 读累计总计数(主上下文) ---------- */
int32_t Encoder_GetTotalCount(void)
{
    uint16_t now = Encoder_GetRawCount();

    /* 用 int16_t 做差,自动处理 16 位回绕
     * 只要两次调用之间变化不超过 ±32767 就正确 */
    int16_t diff = (int16_t)(now - s_last_raw);

    s_total_count += diff;
    s_last_raw = now;
    s_last_delta = diff;

    return s_total_count;
}

/* ---------- 读增量 ---------- */
int16_t Encoder_GetDeltaCount(void)
{
    return s_last_delta;
}

/* ---------- 读方向 ---------- */
int8_t Encoder_GetDirection(void)
{
    if (s_last_delta > 0)
        return 1;
    if (s_last_delta < 0)
        return -1;
    return 0;
}

/* =====================================================================
 *                  ISR 采样:Encoder_Update()(TIM6,10ms)
 * ===================================================================== */
void Encoder_Update(void)
{
    uint32_t now_cyc = bsp_cycle_count();
    uint16_t now_raw = Encoder_GetRawCount();

    /* uint16 差值回绕安全(TIM2 为 32 位计数,截断到低 16 位;
     * 10ms 内 ±32767 计数 ≈ ±209800 RPM 上限,实际远达不到) */
    int16_t diff = (int16_t)(uint16_t)(now_raw - s_rpm_last_raw);
    s_rpm_last_raw = now_raw;

    s_acc_counts += diff;
    if (diff != 0)
        s_last_pulse_cyc = now_cyc;

    /* 静止判定:超过 STOP_TIMEOUT 无任何脉冲 → 输出清零、窗口重同步 */
    if (bsp_cycles_to_us(now_cyc - s_last_pulse_cyc) > ENCODER_STOP_TIMEOUT_MS * 1000u)
    {
        s_acc_counts = 0;
        s_win_start_cyc = now_cyc;
        s_rpm_raw = 0.0f;
        s_rpm_cached = 0.0f;
        for (uint8_t i = 0; i < ENCODER_RPM_FILTER_N; i++)
            s_rpm_buf[i] = 0.0f;
        s_rpm_idx = 0;
        s_rpm_cnt = 0;
        s_dbg_stops++;
        return;
    }

    /* 窗口实测时长(DWT 精确计时,与中断抖动无关) */
    uint32_t elapsed_us = bsp_cycles_to_us(now_cyc - s_win_start_cyc);
    if (elapsed_us == 0)
        elapsed_us = 1; /* 除零保护(理论上首个窗口不会关这么早) */

    int32_t abs_acc = (s_acc_counts < 0) ? -s_acc_counts : s_acc_counts;

    /* 关窗条件:计数够了(高速→每 10ms 关一次)或窗口到上限(低速) */
    if (abs_acc >= ENCODER_MIN_COUNTS_PER_WINDOW ||
        elapsed_us >= ENCODER_MAX_WINDOW_MS * 1000u)
    {
        /* RPM = SIGN * counts / CPR * (60e6 / elapsed_us) */
        float rpm = (float)(ENCODER_SIGN * s_acc_counts) * 60000000.0f /
                    (ENCODER_COUNTS_PER_REV * (float)elapsed_us);

        s_rpm_raw = rpm;

        /* 滑动平均:只在关窗时推入一次(窗口延长期间不重复推) */
        s_rpm_buf[s_rpm_idx] = rpm;
        s_rpm_idx = (uint8_t)((s_rpm_idx + 1) % ENCODER_RPM_FILTER_N);
        if (s_rpm_cnt < ENCODER_RPM_FILTER_N)
            s_rpm_cnt++;

        float sum = 0.0f;
        for (uint8_t i = 0; i < s_rpm_cnt; i++)
            sum += s_rpm_buf[i];
        s_rpm_cached = sum / (float)s_rpm_cnt;

        /* 重开窗口 */
        s_acc_counts = 0;
        s_win_start_cyc = now_cyc;
        s_dbg_windows++;
    }
    /* 否则继续累加 —— 低速窗口扩展 */
}

/* ---------- 调试:打印内部状态(仅主上下文调用) ---------- */
void Encoder_DebugDump(void)
{
    uint32_t now_cyc = bsp_cycle_count();
    printf("CNT=%u acc=%ld win=%lums pulse=%lums ago\r\n",
           (unsigned)Encoder_GetRawCount(),
           (long)s_acc_counts,
           (unsigned long)(bsp_cycles_to_us(now_cyc - s_win_start_cyc) / 1000u),
           (unsigned long)(bsp_cycles_to_us(now_cyc - s_last_pulse_cyc) / 1000u));
    printf("raw=%.2f cached=%.2f flt=%u/%u win#=%lu stop#=%lu\r\n",
           (double)s_rpm_raw, (double)s_rpm_cached,
           (unsigned)s_rpm_cnt, (unsigned)ENCODER_RPM_FILTER_N,
           (unsigned long)s_dbg_windows, (unsigned long)s_dbg_stops);
}

/* ---------- 读转速缓存(任何上下文) ---------- */
float Encoder_GetSpeedRPM(void)
{
    return s_rpm_cached;
}

float Encoder_GetSpeedRPM_Raw(void)
{
    return s_rpm_raw;
}

/* ---------- 重置(可从主上下文调用,临界区防 ISR 交错) ---------- */
void Encoder_Reset(void)
{
    uint32_t pm = __get_PRIMASK();
    __disable_irq();

    __HAL_TIM_SET_COUNTER(ENCODER_TIM, 0);

    s_total_count = 0;
    s_last_raw = 0;
    s_last_delta = 0;

    uint32_t now_cyc = bsp_cycle_count();
    s_rpm_last_raw = 0;
    s_acc_counts = 0;
    s_win_start_cyc = now_cyc;
    s_last_pulse_cyc = now_cyc;
    s_rpm_raw = 0.0f;
    s_rpm_cached = 0.0f;
    for (uint8_t i = 0; i < ENCODER_RPM_FILTER_N; i++)
        s_rpm_buf[i] = 0.0f;
    s_rpm_idx = 0;
    s_rpm_cnt = 0;

    __set_PRIMASK(pm);
}

/* ---------- 设置累计值 ---------- */
void Encoder_SetTotalCount(int32_t count)
{
    s_total_count = count;
}
