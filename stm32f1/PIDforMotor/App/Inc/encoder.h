#ifndef __ENCODER_H
#define __ENCODER_H

#include "main.h"

/* 编码器参数：JGA25370 */
#define ENCODER_PPR 11           // 编码器线数（电机轴每圈脉冲数）
#define ENCODER_MULTIPLIER 4     // 4 倍频（TI1 and TI2 模式）
#define ENCODER_GEAR_RATIO 21.3f // 减速比

/* 输出轴转一圈的总计数 = 11 * 4 * 21.3 = 937.2 */
#define ENCODER_COUNTS_PER_REV \
    (ENCODER_PPR * ENCODER_MULTIPLIER * ENCODER_GEAR_RATIO)

/* 转速滤波窗口大小（越大越平滑，响应越慢） */
#define ENCODER_RPM_FILTER_N 8

/* 静止超时（ms）：超过此时间没有新脉冲则判为 0 RPM */
#define ENCODER_STOP_TIMEOUT_MS 300

/* ---------- 初始化 ---------- */
void Encoder_Init(void);

/* ---------- 读取 ---------- */

/* 读原始 CNT 寄存器值（0~65535） */
uint16_t Encoder_GetRawCount(void);

/* 读累计总计数（int32_t，可正可负，已处理溢出）
 * 注意：每次调用会更新内部状态，建议固定周期调用 */
int32_t Encoder_GetTotalCount(void);

/* 读上次调用 GetTotalCount 时的增量（带符号） */
int16_t Encoder_GetDeltaCount(void);

/* 读方向：1 正转，-1 反转，0 静止 */
int8_t Encoder_GetDirection(void);

/* 读转速（RPM，输出轴），带滑动平均滤波
 * dt_ms：距上次调用的时间（毫秒），建议 10~50ms
 * 返回：输出轴转速，正数正转、负数反转 */
float Encoder_GetSpeedRPM(uint32_t dt_ms);

/* 读转速原始值（未滤波），调试用
 * dt_ms：距上次调用的时间（毫秒） */
float Encoder_GetSpeedRPM_Raw(uint32_t dt_ms);

/* ---------- 辅助 ---------- */

/* 清零累计计数和硬件 CNT */
void Encoder_Reset(void);

/* 设置累计计数值（用于多圈定位） */
void Encoder_SetTotalCount(int32_t count);

#endif