/**
 * @file    app.h
 * @brief   应用层:裸机超级循环逻辑 + 定时器回调
 */

#ifndef __APP_H
#define __APP_H

#ifdef __cplusplus
extern "C" {
#endif
#include "CAN.h"
/* 应用层初始化:在 BSP_Init 之后调用一次 */
void APP_Init(void);

/* 应用层主逻辑:主循环每轮调用一次 */
void APP_Loop(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_H */
