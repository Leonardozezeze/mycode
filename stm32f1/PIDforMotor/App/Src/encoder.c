#include "encoder.h"

/* 根据你的 CubeMX 配置修改 */
extern TIM_HandleTypeDef htim2;
#define ENCODER_TIM (&htim2)

/* ---------- 内部状态（累计计数用） ---------- */
static int32_t s_total_count = 0;
static uint16_t s_last_raw = 0;
static int16_t s_last_delta = 0;

/* ---------- 内部状态（RPM 专用，独立于累计计数） ---------- */
static uint16_t s_rpm_last_raw = 0;
static uint32_t s_rpm_last_tick = 0; // 上次有脉冲的时间戳
static float s_rpm_last_val = 0.0f;  // 上次输出的 RPM

/* ---------- 滑动平均缓冲 ---------- */
static float s_rpm_buf[ENCODER_RPM_FILTER_N] = {0};
static uint8_t s_rpm_idx = 0;
static uint8_t s_rpm_cnt = 0;

/* ---------- 初始化 ---------- */
void Encoder_Init(void)
{
    HAL_TIM_Encoder_Start(ENCODER_TIM, TIM_CHANNEL_ALL);
    __HAL_TIM_SET_COUNTER(ENCODER_TIM, 0);

    s_total_count = 0;
    s_last_raw = 0;
    s_last_delta = 0;

    s_rpm_last_raw = 0;
    s_rpm_last_tick = HAL_GetTick();
    s_rpm_last_val = 0.0f;

    for (uint8_t i = 0; i < ENCODER_RPM_FILTER_N; i++)
        s_rpm_buf[i] = 0.0f;
    s_rpm_idx = 0;
    s_rpm_cnt = 0;
}

/* ---------- 读原始值 ---------- */
uint16_t Encoder_GetRawCount(void)
{
    return (uint16_t)__HAL_TIM_GET_COUNTER(ENCODER_TIM);
}

/* ---------- 读累计总计数 ---------- */
int32_t Encoder_GetTotalCount(void)
{
    uint16_t now = Encoder_GetRawCount();

    /* 用 int16_t 做差，自动处理 16 位溢出
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

/* ---------- 读转速原始值（未滤波） ---------- */
float Encoder_GetSpeedRPM_Raw(uint32_t dt_ms)
{
    if (dt_ms == 0)
        return s_rpm_last_val;

    uint16_t now_raw = Encoder_GetRawCount();
    int16_t diff = (int16_t)(now_raw - s_rpm_last_raw);
    s_rpm_last_raw = now_raw;

    /* 本次有脉冲，刷新时间戳 */
    if (diff != 0)
        s_rpm_last_tick = HAL_GetTick();

    /* 静止判定：长时间没有脉冲，直接返回 0 */
    if ((HAL_GetTick() - s_rpm_last_tick) > ENCODER_STOP_TIMEOUT_MS)
    {
        s_rpm_last_val = 0.0f;
        return 0.0f;
    }

    /* RPM = diff / CPR * (60000 / dt_ms)
     * diff：本周期计数增量（有符号，正转正、反转负）
     * CPR ：输出轴转一圈的总计数 */
    float rpm = (float)diff * 60000.0f / (ENCODER_COUNTS_PER_REV * (float)dt_ms);

    s_rpm_last_val = rpm;
    return rpm;
}

/* ---------- 读转速（带滑动平均滤波） ---------- */
float Encoder_GetSpeedRPM(uint32_t dt_ms)
{
    float rpm_raw = Encoder_GetSpeedRPM_Raw(dt_ms);

    /* 停止状态直接返回 0，且清空滤波器，避免残留值 */
    if (rpm_raw == 0.0f)
    {
        for (uint8_t i = 0; i < ENCODER_RPM_FILTER_N; i++)
            s_rpm_buf[i] = 0.0f;
        s_rpm_idx = 0;
        s_rpm_cnt = 0;
        return 0.0f;
    }

    /* 写入缓冲区 */
    s_rpm_buf[s_rpm_idx] = rpm_raw;
    s_rpm_idx = (s_rpm_idx + 1) % ENCODER_RPM_FILTER_N;
    if (s_rpm_cnt < ENCODER_RPM_FILTER_N)
        s_rpm_cnt++;

    /* 求平均（只对已填入的项求平均，避免启动阶段被 0 拖低） */
    float sum = 0.0f;
    for (uint8_t i = 0; i < s_rpm_cnt; i++)
        sum += s_rpm_buf[i];

    return sum / (float)s_rpm_cnt;
}

/* ---------- 重置 ---------- */
void Encoder_Reset(void)
{
    __HAL_TIM_SET_COUNTER(ENCODER_TIM, 0);

    s_total_count = 0;
    s_last_raw = 0;
    s_last_delta = 0;

    s_rpm_last_raw = 0;
    s_rpm_last_tick = HAL_GetTick();
    s_rpm_last_val = 0.0f;

    for (uint8_t i = 0; i < ENCODER_RPM_FILTER_N; i++)
        s_rpm_buf[i] = 0.0f;
    s_rpm_idx = 0;
    s_rpm_cnt = 0;
}

/* ---------- 设置累计值 ---------- */
void Encoder_SetTotalCount(int32_t count)
{
    s_total_count = count;
}