/**
 * @file    app.h
 * @brief   应用层:FreeRTOS 任务 + 定时器回调
 */

#ifndef __APP_H
#define __APP_H

#ifdef __cplusplus
extern "C" {
#endif

/* 应用层初始化:创建所有任务(在 BSP_Init 之后调用) */
void APP_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_H */
