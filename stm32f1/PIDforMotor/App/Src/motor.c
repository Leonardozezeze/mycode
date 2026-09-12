#include "motor.h"

/* 根据你的 CubeMX 配置修改以下宏 */
extern TIM_HandleTypeDef htim3; // PWM 定时器

#define MOTOR_PWM_TIM (&htim3)
#define MOTOR_PWM_CHANNEL TIM_CHANNEL_1

#define MOTOR_AIN1_PORT GPIOA
#define MOTOR_AIN1_PIN GPIO_PIN_4

#define MOTOR_AIN2_PORT GPIOA
#define MOTOR_AIN2_PIN GPIO_PIN_5

/* 如果你把 STBY 接了 GPIO，就取消下面注释并改成你的引脚
 * 如果 STBY 直接接 3.3V，这两个宏留空即可
 */
// #define MOTOR_STBY_PORT    GPIOA
// #define MOTOR_STBY_PIN     GPIO_PIN_6

#define MOTOR_PWM_MAX 1000 // 对应 ARR+1，你配置的是 ARR=99，这里应是100
                           // 如果你要 0~1000 分辨率，需要把 ARR 改成 999

/* ---------- 内部函数 ---------- */
static void Motor_SetPWM(uint16_t pwm)
{
    if (pwm > MOTOR_PWM_MAX)
        pwm = MOTOR_PWM_MAX;
    __HAL_TIM_SET_COMPARE(MOTOR_PWM_TIM, MOTOR_PWM_CHANNEL, pwm);
}

static void Motor_SetDirection(Motor_Dir_t dir)
{
    if (dir == MOTOR_DIR_CW)
    {
        HAL_GPIO_WritePin(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN, GPIO_PIN_RESET);
    }
    else
    {
        HAL_GPIO_WritePin(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN, GPIO_PIN_SET);
    }
}

/* ---------- 对外接口 ---------- */
void Motor_Init(void)
{
    /* 启动 PWM 输出 */
    HAL_TIM_PWM_Start(MOTOR_PWM_TIM, MOTOR_PWM_CHANNEL);

    /* 默认停止 */
    Motor_Stop();

    /* 使能 TB6612 */
    Motor_Enable();
}

void Motor_Enable(void)
{
#ifdef MOTOR_STBY_PIN
    HAL_GPIO_WritePin(MOTOR_STBY_PORT, MOTOR_STBY_PIN, GPIO_PIN_SET);
#endif
}

void Motor_Disable(void)
{
#ifdef MOTOR_STBY_PIN
    HAL_GPIO_WritePin(MOTOR_STBY_PORT, MOTOR_STBY_PIN, GPIO_PIN_RESET);
#endif
}

void Motor_Forward(uint16_t speed)
{
    Motor_SetDirection(MOTOR_DIR_CW);
    Motor_SetPWM(speed);
}

void Motor_Reverse(uint16_t speed)
{
    Motor_SetDirection(MOTOR_DIR_CCW);
    Motor_SetPWM(speed);
}

void Motor_Stop(void)
{
    /* 先关 PWM，再断方向，避免瞬间反向电流 */
    Motor_SetPWM(0);
    HAL_GPIO_WritePin(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN, GPIO_PIN_RESET);
}

void Motor_Brake(void)
{
    Motor_SetPWM(0);
    HAL_GPIO_WritePin(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN, GPIO_PIN_SET);
}

void Motor_Set(int16_t speed)
{
    if (speed > 0)
    {
        Motor_Forward((uint16_t)speed);
    }
    else if (speed < 0)
    {
        Motor_Reverse((uint16_t)(-speed));
    }
    else
    {
        Motor_Stop();
    }
}