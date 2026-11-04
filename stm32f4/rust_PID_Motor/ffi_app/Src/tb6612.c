#include "stm32f4xx.h"
#include "tb6612.h"

/* 根据你的 CubeMX 配置修改以下宏 */
extern TIM_HandleTypeDef htim3; // PWM 定时器

#define TB6612_PWM_TIM (&htim3)
#define TB6612_PWM_CHANNEL TIM_CHANNEL_1

#define TB6612_AIN1_PORT GPIOA
#define TB6612_AIN1_PIN GPIO_PIN_4

#define TB6612_AIN2_PORT GPIOA
#define TB6612_AIN2_PIN GPIO_PIN_5

/* 如果把 STBY 接了 GPIO，就取消下面注释并改成对应引脚
 * 如果 STBY 直接接 3.3V，保持注释即可（enable/disable 为空操作）
 */
// #define TB6612_STBY_PORT GPIOA
// #define TB6612_STBY_PIN  GPIO_PIN_6

/* ========================= 芯片级原语（供 Rust FFI）=========================
 * 只做寄存器写入和 TB6612 真值表译码；更高层的策略
 * （斜坡、软启动、堵转保护……）全在 Rust 侧，不用动这个文件。
 */

void tb6612_init(void)
{
    tb6612_enable();
    /* 先置入已知安全状态，再启动 PWM 输出 */
    tb6612_set_pwm(0);
    tb6612_set_dir(0, 0);
    HAL_TIM_PWM_Start(TB6612_PWM_TIM, TB6612_PWM_CHANNEL);
}

void tb6612_set_pwm(uint16_t pwm)
{
    if (pwm > TB6612_PWM_MAX)
        pwm = TB6612_PWM_MAX;
    __HAL_TIM_SET_COMPARE(TB6612_PWM_TIM, TB6612_PWM_CHANNEL, pwm);
}

void tb6612_set_dir(uint8_t ain1, uint8_t ain2)
{
    HAL_GPIO_WritePin(TB6612_AIN1_PORT, TB6612_AIN1_PIN,
                      ain1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TB6612_AIN2_PORT, TB6612_AIN2_PIN,
                      ain2 ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void tb6612_enable(void)
{
#ifdef TB6612_STBY_PIN
    HAL_GPIO_WritePin(TB6612_STBY_PORT, TB6612_STBY_PIN, GPIO_PIN_SET);
#endif
}

void tb6612_disable(void)
{
#ifdef TB6612_STBY_PIN
    HAL_GPIO_WritePin(TB6612_STBY_PORT, TB6612_STBY_PIN, GPIO_PIN_RESET);
#endif
}

/* 带符号调速：正 = 正转（AIN1=1,AIN2=0），负 = 反转（AIN1=0,AIN2=1）
 * 幅值钳位在 set_pwm 里做；先转成 int32 再取负，i16 最负值（-32768）也安全 */
void tb6612_set_speed(int16_t speed)
{
    if (speed >= 0)
    {
        tb6612_set_dir(1, 0);
        tb6612_set_pwm((uint16_t)speed);
    }
    else
    {
        tb6612_set_dir(0, 1);
        tb6612_set_pwm((uint16_t)-(int32_t)speed);
    }
}
