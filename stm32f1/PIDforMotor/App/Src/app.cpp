#include "app.h"
#include "motor.h"
#include "encoder.h"
#include "pid.h"
#include "shell.h"
#include "cli_motor.h"

PID motor_pid;
uint16_t pwm = 0;

/* ========== 私有变量 ========== */
extern ADC_HandleTypeDef hadc1;

/* ========== 初始化 ========== */
void App_Init(void)
{
    BSP_Init();
    Motor_Init();
    Encoder_Init();
    Motor_Set(pwm);
    motor_pid.init(0.1, 0.1, 0, 0, 1000);
    printf("App init done\r\n");
    shell_init();
    motor_cli_register();
}

/* ========== 主循环 ========== */
void App_Loop(void)
{
    int c = ringbuf_pop(&g_rx);
    if (c >= 0)
    {
        shell_feed((char)c);
    }
    pwm=motor_pid.compute();
    Motor_Set(pwm);
    uint16_t rpm=Encoder_GetSpeedRPM(1);
    motor_pid.setActual(rpm);
    delay_ms(1);
}
