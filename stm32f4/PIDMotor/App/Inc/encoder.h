#ifndef __ENCODER_H
#define __ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* ========================= 编码器参数 ========================= */
#define ENCODER_PPR 11           // 编码器线数(电机轴每圈脉冲数)
#define ENCODER_MULTIPLIER 4     // 4倍频(TI1 and TI2 模式)
#define ENCODER_GEAR_RATIO 21.3f // 减速比

/* 输出轴转一圈的总计数 = 11 * 4 * 21.3 = 937.2(小数,禁止整数化) */
#define ENCODER_COUNTS_PER_REV \
    (ENCODER_PPR * ENCODER_MULTIPLIER * ENCODER_GEAR_RATIO)

/* 编码器方向符号:正 PWM(Motor_Forward)时若 Speed 读数为负,改为 -1
 * (等效于对调编码器 A/B 相接线;符号反了 PID 会正反馈失控满速) */
#define ENCODER_SIGN -1

/* ========================= 测速参数 ========================= */

/* 测频法采样周期(ms):使用较长周期减少低速量化误差 */
#define ENCODER_SAMPLE_PERIOD_MS 100

/* 静止超时(ms):超过此时间没有新脉冲则判为 0 RPM */
#define ENCODER_STOP_TIMEOUT_MS 300

/* ========================= 滤波参数 ========================= */

/* 一阶低通滤波系数(IIR):y[n] = α·x[n] + (1-α)·y[n-1]
 * α 越小滤波越强(响应越慢),α=1 时无滤波
 * 推荐范围:0.1~0.5,根据实际转速波动调整 */
#define ENCODER_FILTER_ALPHA 0.3f

/* ========================= 初始化 ========================= */

/* 初始化编码器硬件和测速状态 */
void Encoder_Init(void);

/* ========================= 测速接口 ========================= */

/* 统一更新接口(当前使用测频法,由 TIM6 中断每 10ms 调用) */
void Encoder_Update(void);

/* 测频法(M法):固定时间窗口内计数脉冲数,适合高速 */
void Encoder_Update_M(void);

/* 测周法(T法):测量相邻脉冲的时间间隔,适合低速 */
void Encoder_Update_T(void);

/* ========================= 读取接口 ========================= */

/* 读原始 CNT 寄存器值 */
uint16_t Encoder_GetRawCount(void);

/* 读转速(输出轴 RPM,带符号,当前使用测频法) */
float Encoder_GetSpeedRPM(void);

/* 读测频法原始转速(滤波前,调试用) */
float Encoder_GetSpeedRPM_M(void);

/* 读测周法原始转速(滤波前,调试用) */
float Encoder_GetSpeedRPM_T(void);

/* ========================= 滤波配置 ========================= */

/* 运行时调整滤波系数(0~1,1=无滤波) */
void Encoder_SetFilterAlpha(float alpha);

/* 读当前滤波系数 */
float Encoder_GetFilterAlpha(void);

/* ========================= 调试接口 ========================= */

/* 调试:打印测速内部状态 */
void Encoder_DebugDump(void);

#ifdef __cplusplus
}
#endif

#endif
