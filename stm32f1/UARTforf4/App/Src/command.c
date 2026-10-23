/**
 * @file    command.c
 * @brief   F4 PID 电机控制命令实现
 */

#include "command.h"
#include "bsp.h"
#include <stdarg.h>
#include <stdio.h>

/* ==============================================================================
 * 命令初始化
 * ============================================================================== */
void Command_Init(void)
{
    /* 目前无需额外初始化,保留作为扩展接口 */
    /* 如需在发送前添加延时或握手协议,可在此实现 */
}

/* ==============================================================================
 * 通用命令发送
 * ============================================================================== */
void Command_Send(const char *fmt, ...)
{
    char buf[128];
    va_list ap;

    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (n > 0) {
        if (n > (int)sizeof(buf) - 1) {
            n = sizeof(buf) - 1;
        }
        /* 确保以 \r\n 结尾 */
        if (n >= 2) {
            if (buf[n-2] != '\r' || buf[n-1] != '\n') {
                if (n < (int)sizeof(buf) - 2) {
                    buf[n++] = '\r';
                    buf[n++] = '\n';
                    buf[n] = '\0';
                }
            }
        }
        HAL_UART_Transmit(&huart2, (uint8_t *)buf, n, HAL_MAX_DELAY);
    }
}

/* ==============================================================================
 * 电机 PID 开关控制
 * ============================================================================== */
void Command_MotorPID_Switch(cmd_switch_t state)
{
    if (state == CMD_ON) {
        Command_MotorPID_On();
    } else {
        Command_MotorPID_Off();
    }
}

void Command_MotorPID_On(void)
{
    UART2_SendFormatted("motor pid on\r\n");
}

void Command_MotorPID_Off(void)
{
    UART2_SendFormatted("motor pid off\r\n");
}

/* ==============================================================================
 * 电机 PID 速度设置
 * ============================================================================== */
void Command_MotorPID_SetSpeed(int32_t speed)
{
    UART2_SendFormatted("motor pid speed %d\r\n", speed);
}

/* ==============================================================================
 * 电机 PID 参数设置 (Kp, Ki, Kd)
 * ============================================================================== */
void Command_MotorPID_SetParams(float kp, float ki, float kd)
{
    UART2_SendFormatted("motor pid set %f %f %f\r\n", kp, ki, kd);
}

/* ==============================================================================
 * 电机 PID 状态查询
 * ============================================================================== */
void Command_MotorPID_QueryStatus(void)
{
    UART2_SendFormatted("motor pid status\r\n");
}

/* ==============================================================================
 * 电机急停
 * ============================================================================== */
void Command_Motor_EmergencyStop(void)
{
    UART2_SendFormatted("motor stop\r\n");
}

/* ==============================================================================
 * 自定义命令
 * 如需发送自定义命令,直接使用 UART2_SendFormatted() 即可
 * 例如: UART2_SendFormatted("custom command %d\r\n", value);
 * ============================================================================== */
