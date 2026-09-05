/**
 * @file    app.c
 * @brief   应用层装配:调用 Drivers/User 下各功能模块
 *
 * 具体实现已下沉到 Drivers/User:
 *   - canopen_usr.c : CANopen 协议栈集成 + 业务逻辑
 *                     (init / process / timer_isr / LED 指示 / 应用变量)
 *
 * 本文件只负责按顺序装配,并承接 HAL 中断回调做分发。
 */

#include "main.h"
#include "app.h"
#include "canopen_usr.h"

/* 应用层初始化:在 BSP_Init() 之后调用一次 */
void APP_Init(void)
{
    CANopen_Init();
}

/* 应用层主循环:在 main() 的 while(1) 中反复调用 */
void APP_Loop(void)
{
    CANopen_run();
    /* 3. 短暂延时,降低 CPU 占用 */
    delay_ms(10);
}

/**
 * @brief  TIM4 1ms 周期中断回调
 *
 * 分发给 CANopen 实时任务(SYNC / PDO 由 co_timer_isr 处理)。
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == &htim4) {
        co_timer_isr();
    }
}
