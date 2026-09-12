#ifndef __PID_H
#define __PID_H

#include <stdint.h>

typedef struct
{
    /* PID 参数 */
    float Kp;
    float Ki;
    float Kd;

    /* 内部状态 */
    float e;      // 当前误差
    float e_last; // 上次误差
    float e_prev; // 上上次误差

    /* 输出 */
    float out; // 当前输出（累加值）

    /* 限幅 */
    float out_min; // 输出下限，如 0
    float out_max; // 输出上限，如 99
} PID_t;

/* 初始化 */
void PID_Init(PID_t *pid, float Kp, float Ki, float Kd,
              float out_min, float out_max);

/* 计算，输入目标值和实际值，返回新的输出 */
float PID_Compute(PID_t *pid, float target, float actual);

/* 重置内部状态 */
void PID_Reset(PID_t *pid);

#endif