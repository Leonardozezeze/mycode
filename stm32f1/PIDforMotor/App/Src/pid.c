#include "pid.h"

void PID_Init(PID_t *pid, float Kp, float Ki, float Kd,
              float out_min, float out_max)
{
    pid->Kp = Kp;
    pid->Ki = Ki;
    pid->Kd = Kd;

    pid->e = 0;
    pid->e_last = 0;
    pid->e_prev = 0;

    pid->out = 0;
    pid->out_min = out_min;
    pid->out_max = out_max;
}

float PID_Compute(PID_t *pid, float target, float actual)
{
    /* 1. 计算当前误差 */
    pid->e = target - actual;

    /* 2. 增量式 PID 公式 */
    float delta = pid->Kp * (pid->e - pid->e_last) + pid->Ki * pid->e + pid->Kd * (pid->e - 2.0f * pid->e_last + pid->e_prev);

    /* 3. 累加到输出 */
    pid->out += delta;

    /* 4. 输出限幅 */
    if (pid->out > pid->out_max)
        pid->out = pid->out_max;
    if (pid->out < pid->out_min)
        pid->out = pid->out_min;

    /* 5. 保存历史误差 */
    pid->e_prev = pid->e_last;
    pid->e_last = pid->e;

    return pid->out;
}

void PID_Reset(PID_t *pid)
{
    pid->e = 0;
    pid->e_last = 0;
    pid->e_prev = 0;
    pid->out = 0;
}