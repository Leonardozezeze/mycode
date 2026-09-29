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

/* ---------- 测速模式 ---------- */
static Encoder_SpeedMode_t s_speed_mode = ENCODER_SPEED_MODE_AUTO;

/* ---------- 测周法（T法）专用状态 ---------- */
static uint32_t s_t_last_pulse_tick = 0;   // 上一个脉冲的时间戳（us）
static uint16_t s_t_last_pulse_raw = 0;    // 上一个脉冲的计数器值
static float s_t_last_rpm = 0.0f;          // 上次计算的 RPM
static uint8_t s_t_first_pulse = 1;        // 首次脉冲标志

/* ---------- 微秒时间戳获取（使用 DWT 或定时器） ---------- */
static inline uint32_t Encoder_GetMicros(void)
{
    /* 使用 HAL_GetTick() * 1000 作为近似微秒值
     * 如果需要更高精度，可以使用 DWT 周期计数器 */
    return HAL_GetTick() * 1000;
}

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

    /* 测速模式初始化 */
    s_speed_mode = ENCODER_SPEED_MODE_AUTO;

    /* 测周法状态初始化 */
    s_t_last_pulse_tick = Encoder_GetMicros();
    s_t_last_pulse_raw = 0;
    s_t_last_rpm = 0.0f;
    s_t_first_pulse = 1;

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

/* ---------- 读转速原始值（未滤波，根据模式选择测速方法） ---------- */
float Encoder_GetSpeedRPM_Raw(uint32_t dt_ms)
{
    switch (s_speed_mode)
    {
    case ENCODER_SPEED_MODE_M:
        return Encoder_GetSpeedRPM_M(dt_ms);

    case ENCODER_SPEED_MODE_T:
        return Encoder_GetSpeedRPM_T();

    case ENCODER_SPEED_MODE_AUTO:
        return Encoder_GetSpeedRPM_MT(dt_ms);

    default:
        return Encoder_GetSpeedRPM_M(dt_ms);
    }
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

    /* 重置测周法状态 */
    s_t_last_pulse_tick = Encoder_GetMicros();
    s_t_last_pulse_raw = 0;
    s_t_last_rpm = 0.0f;
    s_t_first_pulse = 1;

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

/* =====================================================================
 *                         测频法（M 法）
 * =====================================================================
 * 原理：在固定时间 T 内计数脉冲数 m1
 * 公式：RPM = m1 / CPR * (60000 / T_ms)
 * 特点：高速时精度高，低速时误差大
 * ===================================================================== */

/* ---------- 测频法测速（M 法） ---------- */
float Encoder_GetSpeedRPM_M(uint32_t dt_ms)
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

    /* RPM = diff / CPR * (60000 / dt_ms) */
    float rpm = (float)diff * 60000.0f / (ENCODER_COUNTS_PER_REV * (float)dt_ms);

    s_rpm_last_val = rpm;
    return rpm;
}

/* =====================================================================
 *                         测周法（T 法）
 * =====================================================================
 * 原理：测量相邻脉冲的时间间隔 T
 * 公式：RPM = 60000000 / (CPR * T_us)
 * 特点：低速时精度高，高速时误差大（因为时间间隔短）
 * ===================================================================== */

/* ---------- 测周法测速（T 法） ---------- */
float Encoder_GetSpeedRPM_T(void)
{
    uint32_t now_us = Encoder_GetMicros();
    uint16_t now_raw = Encoder_GetRawCount();

    /* 检测是否有新脉冲（计数器值变化） */
    int16_t diff = (int16_t)(now_raw - s_t_last_pulse_raw);

    if (diff != 0)
    {
        /* 有脉冲，计算时间间隔 */
        uint32_t interval_us = now_us - s_t_last_pulse_tick;

        /* 更新状态 */
        s_t_last_pulse_tick = now_us;
        s_t_last_pulse_raw = now_raw;

        /* 首次脉冲只记录，不计算 */
        if (s_t_first_pulse)
        {
            s_t_first_pulse = 0;
            return 0.0f;
        }

        /* 有效时间间隔才计算 RPM */
        if (interval_us > 0 && interval_us < ENCODER_T_METHOD_MAX_INTERVAL_MS * 1000UL)
        {
            /* RPM = 60000000 / (CPR * T_us)
             * 60000000 = 60 * 1000000 (秒转分钟 * 微秒) */
            float rpm = 60000000.0f / (ENCODER_COUNTS_PER_REV * (float)interval_us);

            /* 根据方向设置符号 */
            if (diff < 0)
                rpm = -rpm;

            s_t_last_rpm = rpm;
        }
    }
    else
    {
        /* 无脉冲，检查是否超时 */
        uint32_t interval_us = now_us - s_t_last_pulse_tick;
        if (interval_us > ENCODER_T_METHOD_MAX_INTERVAL_MS * 1000UL)
        {
            /* 超时，认为停止 */
            s_t_last_rpm = 0.0f;
        }
    }

    return s_t_last_rpm;
}

/* =====================================================================
 *                      自动切换（M/T 法）
 * =====================================================================
 * 原理：高速时用测频法（M法），低速时用测周法（T法）
 * 切换条件：当测频法计数小于阈值时切换到测周法
 * ===================================================================== */

/* ---------- 自动切换测速（M/T 法） ---------- */
float Encoder_GetSpeedRPM_MT(uint32_t dt_ms)
{
    /* 读取当前状态用于判断 */
    uint16_t now_raw = Encoder_GetRawCount();
    int16_t diff = (int16_t)(now_raw - s_rpm_last_raw);

    /* 判断速度范围，选择测速方法 */
    int16_t abs_diff = (diff >= 0) ? diff : -diff;

    float rpm;
    if (abs_diff >= ENCODER_AUTO_SWITCH_THRESHOLD)
    {
        /* 高速：使用测频法，直接计算 */
        if (dt_ms == 0)
        {
            rpm = s_rpm_last_val;
        }
        else
        {
            /* 更新状态 */
            s_rpm_last_raw = now_raw;
            if (diff != 0)
                s_rpm_last_tick = HAL_GetTick();

            /* 静止判定 */
            if ((HAL_GetTick() - s_rpm_last_tick) > ENCODER_STOP_TIMEOUT_MS)
            {
                rpm = 0.0f;
            }
            else
            {
                rpm = (float)diff * 60000.0f / (ENCODER_COUNTS_PER_REV * (float)dt_ms);
            }
        }
    }
    else
    {
        /* 低速：使用测周法 */
        rpm = Encoder_GetSpeedRPM_T();

        /* 同步 M 法状态（为下次判断准备） */
        s_rpm_last_raw = now_raw;
        if (diff != 0)
            s_rpm_last_tick = HAL_GetTick();
    }

    s_rpm_last_val = rpm;
    return rpm;
}

/* ---------- 设置测速模式 ---------- */
void Encoder_SetSpeedMode(Encoder_SpeedMode_t mode)
{
    if (mode <= ENCODER_SPEED_MODE_AUTO)
    {
        s_speed_mode = mode;
    }
}

/* ---------- 获取当前测速模式 ---------- */
Encoder_SpeedMode_t Encoder_GetSpeedMode(void)
{
    return s_speed_mode;
}