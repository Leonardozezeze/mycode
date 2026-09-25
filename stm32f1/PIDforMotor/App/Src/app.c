#include "app.h"
#include "motor.h"
#include "encoder.h"
#include "pid.h"
#include "shell.h"
PID_t motor_pid;
uint16_t pwm=0;
/* ========== 私有变量 ========== */
extern ADC_HandleTypeDef hadc1;
/* ========== 初始化 ========== */
void App_Init(void)
{
    BSP_Init();
    shell_init();
    Motor_Init();
    Encoder_Init();
    Motor_Set(pwm);
    PID_Init(&motor_pid,0.1,0.1,0,0,1000);
    printf("App init done\r\n");
}

/* ========== 主循环 ========== */
void App_Loop(void)
{
    int c = ringbuf_pop(&g_rx);
    if (c >= 0)
    {
        shell_feed((char)c); /* 在"主循环上下文"里跑 shell */
    }
    
    float rpm=Encoder_GetSpeedRPM(10);
    pwm=(uint16_t)PID_Compute(&motor_pid,79,rpm);
    Motor_Set(pwm);
    delay_ms(10);
}
