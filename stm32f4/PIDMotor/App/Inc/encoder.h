#ifndef __ENCODER_H
#define __ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* 编码器参数:JGA25370 */
#define ENCODER_PPR 11           // 编码器线数(电机轴每圈脉冲数)
#define ENCODER_MULTIPLIER 4     // 4 倍频(TI1 and TI2 模式)
#define ENCODER_GEAR_RATIO 21.3f // 减速比

/* 输出轴转一圈的总计数 = 11 * 4 * 21.3 = 937.2(小数,禁止整数化) */
#define ENCODER_COUNTS_PER_REV \
    (ENCODER_PPR * ENCODER_MULTIPLIER * ENCODER_GEAR_RATIO)

/* 编码器方向符号:正 PWM(Motor_Forward)时若 Speed 读数为负,改为 -1
 * (等效于对调编码器 A/B 相接线;符号反了 PID 会正反馈失控满速)
 * 本机实测:motor pwm 正值时 Speed = -12.3 RPM → 置 -1 校正 */
#define ENCODER_SIGN -1

/* 转速滤波窗口大小(越大越平滑,响应越慢) */
#define ENCODER_RPM_FILTER_N 8

/* 静止超时(ms):超过此时间没有新脉冲则判为 0 RPM */
#define ENCODER_STOP_TIMEOUT_MS 300

/* ---------- 自适应采样窗口 ----------
 * Encoder_Update() 每 10ms(TIM6 中断)累加计数;窗口内计数达到
 * MIN_COUNTS 才关窗计算转速,否则继续累加(低速时窗口自动变长,
 * 保证量化误差 ≤ ~10%);窗口最长不超过 MAX_WINDOW_MS。 */
#define ENCODER_MIN_COUNTS_PER_WINDOW 10
#define ENCODER_MAX_WINDOW_MS 200

/* ---------- 初始化 / 重置 ---------- */
void Encoder_Init(void);

/* 清零累计计数和硬件 CNT(可从主上下文调用,内部有临界区保护) */
void Encoder_Reset(void);

/* ---------- ISR 采样(仅由 TIM6 中断里的 Control_Task10ms 调用,固定 10ms) ----------
 * 读 TIM2 计数、维护自适应窗口和滑动平均、更新缓存转速。
 * 测速完全由内部时基(DWT 周期计数)驱动,与调用方节奏无关。 */
void Encoder_Update(void);

/* ---------- 读取 ---------- */

/* 读原始 CNT 寄存器值(低 16 位) */
uint16_t Encoder_GetRawCount(void);

/* 读累计总计数(int32_t,可正可负,已处理溢出)
 * 主上下文调用,内部状态与 ISR 测速状态不相交 */
int32_t Encoder_GetTotalCount(void);

/* 读上次调用 GetTotalCount 时的增量(带符号) */
int16_t Encoder_GetDeltaCount(void);

/* 读方向:1 正转,-1 反转,0 静止 */
int8_t Encoder_GetDirection(void);

/* 读转速(输出轴 RPM,带符号):返回中断里算好的缓存值,任何上下文可读。
 * GetSpeedRPM     = 8 点滑动平均后(平滑,供 PID/显示)
 * GetSpeedRPM_Raw = 最近一个窗口的原始值(调试用) */
float Encoder_GetSpeedRPM(void);
float Encoder_GetSpeedRPM_Raw(void);

/* ---------- 辅助 ---------- */

/* 设置累计计数值(用于多圈定位) */
void Encoder_SetTotalCount(int32_t count);

/* 调试:打印测速内部状态(仅主上下文调用,如 CLI 的 motor enc)
 * win/stop 计数用于证明 TIM6 中断活着;CNT/acc 区分编码器与算法问题 */
void Encoder_DebugDump(void);

#ifdef __cplusplus
}
#endif

#endif
