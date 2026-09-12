#ifndef __MOTOR_H
#define __MOTOR_H

#include "main.h" // 包含 HAL 库和 CubeMX 生成的定义

/* 电机方向枚举 */
typedef enum
{
    MOTOR_DIR_CW = 0,  // 正转
    MOTOR_DIR_CCW = 1, // 反转
} Motor_Dir_t;

/* 初始化（启动 PWM、拉高 STBY 等） */
void Motor_Init(void);

/* 设置方向和速度
 * speed: -1000 ~ +1000
 *   正数正转，负数反转，0 停止
 */
void Motor_Set(int16_t speed);

/* 正转，speed 0~1000 */
void Motor_Forward(uint16_t speed);

/* 反转，speed 0~1000 */
void Motor_Reverse(uint16_t speed);

/* 停止（滑行，AIN1=AIN2=0，PWM=0） */
void Motor_Stop(void);

/* 刹车（AIN1=AIN2=1，PWM=0） */
void Motor_Brake(void);

/* 使能/失能芯片 */
void Motor_Enable(void);
void Motor_Disable(void);

#endif