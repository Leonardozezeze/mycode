/**
 * @file    main.c
 * @brief   启动骨架:BSP_Init(硬件) -> APP_Init(应用) -> 超级循环
 */

#include "main.h"
#include "app.h"

int main(void)
{
    BSP_Init();   /* 硬件初始化:时钟 + 外设(见 bsp.c) */
    APP_Init();   /* 应用初始化(见 app.c) */

    while (1) {
        APP_Loop();   /* 裸机超级循环:应用主逻辑(见 app.c) */
    }
}

