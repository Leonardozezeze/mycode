/**
 * @file    command.h
 * @brief   F4 PID 电机控制命令封装
 * @note    通过 UART2 向 F4 发送控制命令
 */

#ifndef __COMMAND_H
#define __COMMAND_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdbool.h>

/**
 * @brief  命令状态枚举
 */
typedef enum {
    CMD_OFF = 0,  // 关闭
    CMD_ON = 1,   // 开启
} cmd_switch_t;

/**
 * @brief  初始化命令模块
 * @note   调用此函数前确保 UART2 已初始化
 */
void Command_Init(void);

/**
 * @brief  发送命令 (通用接口)
 * @param  fmt: 格式化字符串
 * @param  ...: 可变参数
 * @note   内部调用 UART2_SendFormatted,自动添加 \r\n 结尾
 *         用于发送自定义命令或扩展命令
 */
void Command_Send(const char *fmt, ...);

/**
 * @brief  发送电机 PID 开关命令
 * @param  state: CMD_ON 开启PID, CMD_OFF 关闭PID
 * @note   发送格式: "motor pid on\r\n" 或 "motor pid off\r\n"
 */
void Command_MotorPID_Switch(cmd_switch_t state);

/**
 * @brief  开启电机 PID 控制
 * @note   等同于 Command_MotorPID_Switch(CMD_ON)
 */
void Command_MotorPID_On(void);

/**
 * @brief  关闭电机 PID 控制
 * @note   等同于 Command_MotorPID_Switch(CMD_OFF)
 */
void Command_MotorPID_Off(void);

/**
 * @brief  设置电机 PID 目标速度
 * @param  speed: 目标速度值 (整数)
 * @note   发送格式: "motor pid speed <speed>\r\n"
 */
void Command_MotorPID_SetSpeed(int32_t speed);

/**
 * @brief  设置电机 PID 参数
 * @param  kp: 比例系数 (放大100倍后的整数,如 1.5 传 150)
 * @param  ki: 积分系数 (放大100倍后的整数)
 * @param  kd: 微分系数 (放大100倍后的整数)
 * @note   发送格式: "motor pid set <kp> <ki> <kd>\r\n"
 */
void Command_MotorPID_SetParams(float kp, float ki, float kd);

/**
 * @brief  查询电机 PID 状态
 * @note   发送格式: "motor pid status\r\n"
 */
void Command_MotorPID_QueryStatus(void);

/**
 * @brief  急停命令 - 立即停止电机
 * @note   发送格式: "motor stop\r\n"
 */
void Command_Motor_EmergencyStop(void);

#ifdef __cplusplus
}
#endif

#endif /* __COMMAND_H */
