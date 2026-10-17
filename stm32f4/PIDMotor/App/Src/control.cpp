/**
 * @file    control.cpp
 * @brief   10ms 固定周期控制环:TIM6 中断里完成 编码器采样 → PID → PWM 输出
 *
 * 接管方式:仅覆盖 HAL 的 __weak HAL_TIM_PeriodElapsedCallback(全工程唯一定义)。
 * TIM6_DAC_IRQHandler / htim6 / 时钟 / NVIC 全部由 CubeMX 生成的 Core 代码负责,
 * 这里绝不重复定义或重复初始化(否则链接冲突/双重配置)。
 *
 * ISR 卫生规则(违反 = 死机或卡死):
 *   - 严禁 printf / UART HAL(阻塞式 HAL_UART_Transmit,超时依赖 HAL_GetTick);
 *   - 严禁 HAL_Delay 及任何 tick 超时 API:SysTick 优先级 15 无法抢占
 *     TIM6 的优先级 0,tick 在中断里是冻结的;
 *   - 任务保持 O(µs):Motor_Set 是纯寄存器写(motor.c),安全。
 *
 * 并发:CLI(主上下文)写 motor_pid.target/status 均为单个对齐字存储,
 * Cortex-M4 上原子;disable+reset 多字段序列由 CLI 侧 Control_Lock/Unlock 保护。
 */
#include "control.h"
#include "encoder.h"
#include "motor.h"

/* 全局 PID 实例(从 app.cpp 迁入):控制中断与 CLI 共享同一对象 */
PID motor_pid;

static volatile int16_t s_pwm_applied = 0; /* 最后实际写入的 PWM(ISR 或 CLI 直驱) */
static uint8_t s_was_enabled = 0;          /* 上一拍的 PID 使能状态,用于检测 off 边沿 */

/* ---------- 武装控制中断(App_Init 最后一步) ---------- */
void Control_Init(void)
{
    /* MX_TIM6_Init 只配置时基、不开中断;防御性清一次 UIF 再启动,
     * 避免上电残留标志导致武装后立即多进一次中断 */
    __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
    HAL_TIM_Base_Start_IT(&htim6);
}

/* ---------- PWM 缓存读写 ---------- */
int16_t Control_GetPWM(void)
{
    return s_pwm_applied;
}

void Control_SetPWM(int16_t v)
{
    s_pwm_applied = v;
    Motor_Set(v); /* 内部钳位到 ±MOTOR_PWM_MAX */
}

/* ---------- CLI 临界区 ---------- */
void Control_Lock(void)
{
    HAL_NVIC_DisableIRQ(TIM6_DAC_IRQn);
}

void Control_Unlock(void)
{
    HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
}

/* ---------- 10ms 控制任务(中断上下文) ---------- */
static void Control_Task10ms(void)
{
    /* 无论 PID 是否开启都更新测速:
     * CLI 的 motor status 随时有活数据;重新使能 PID 前窗口已同步 */
    Encoder_Update();

    uint8_t en = motor_pid.getStatus();
    if (en)
    {
        motor_pid.setActual(Encoder_GetSpeedRPM()); /* float 全程,无截断 */
        float out = motor_pid.compute();            /* PID 内部钳位 [-1000,+1000] */
        /* 对称四舍五入:+0.5/-0.5(单纯 +0.5f 对负数是向正无穷取整,方向错) */
        s_pwm_applied = (int16_t)(out >= 0.0f ? out + 0.5f : out - 0.5f);
        Motor_Set(s_pwm_applied);                   /* 负值 → AIN 反向,电机反转 */
    }
    else if (s_was_enabled)
    {
        /* PID off 的 1→0 边沿:一次性停机(决策 D2)。
         * 若想保留旧"PWM 冻结、电机续转"行为,删除本分支即可 */
        Motor_Stop();
        s_pwm_applied = 0;
    }
    s_was_enabled = en;
}

/* ---------- HAL 弱回调覆盖(全工程唯一定义,必须 extern "C") ---------- */
extern "C" void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
        Control_Task10ms();
}
