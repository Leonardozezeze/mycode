#ifndef __CONTROL_H
#define __CONTROL_H

/**
 * @file    control.h
 * @brief   10ms 固定周期控制环(TIM6 中断):编码器采样 + PID + PWM 输出
 *
 * 时基来自 CubeMX 配置的 TIM6(PSC=83, ARR=9999 → 84MHz/84/10000 = 10ms)。
 * motor_pid 的所有权在本模块(control.cpp),CLI 通过 extern 声明访问。
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* CubeMX 在 main.c 定义 htim6,但 main.h 不导出句柄(htim2/htim3 同样如此,
 * 参见 encoder.c 的 extern 模式)。此处声明放在 extern "C" 块内,C/C++ 均可用 */
extern TIM_HandleTypeDef htim6;

#define CONTROL_PERIOD_MS 10 /* TIM6 中断周期,与 CubeMX 配置一致 */

/* 武装 10ms 控制中断 —— 必须在 App_Init 的最后一步调用
 * (依赖 BSP_Init 的 DWT、Encoder_Init、motor_pid.init 均已就绪) */
void Control_Init(void);

/* 最后实际写入电机的 PWM 占空比(0~1000,PID 输出或 CLI 手动直驱) */
int16_t Control_GetPWM(void);

/* 手动直驱:Motor_Set + 更新缓存。仅在 PID 关闭时使用(CLI 侧负责守卫,
 * 否则 10ms 内就会被控制中断覆盖) */
void Control_SetPWM(int16_t v);

/* CLI 临界区:屏蔽/恢复 TIM6 中断(用于 disable+reset 这类多字段序列) */
void Control_Lock(void);
void Control_Unlock(void);

#ifdef __cplusplus
}

#include "pid.h"
/* 全局 PID 实例,定义在 control.cpp(注意:C++ 类型,必须在 extern "C" 块之外) */
extern PID motor_pid;
#endif

#endif /* __CONTROL_H */
