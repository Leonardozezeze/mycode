#ifndef __APP_H
#define __APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h" /* 拿到 HAL 和所有外设句柄的 extern */
#include "bsp.h"
void App_Init(void); /* 所有初始化完成后调用一次 */
void App_Loop(void); /* 主循环里反复调用 */

#ifdef __cplusplus
}
#endif

#endif