/**
 * @file    canopen_usr.h
 * @brief   CANopen 协议栈集成模块(用户封装,含业务逻辑)
 *
 * 本模块持有工程中唯一的 CANopenNodeSTM32 实例,
 * 把协议栈的 init / process / timer_isr 统一封装成 co_* 接口,
 * 同时承载 LED 状态指示 + OD 应用变量维护等业务(原 business 并入),
 * 由 Core/Src/app.c 调用。
 */

#ifndef __CANOPEN_USR_H
#define __CANOPEN_USR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "CO_app_STM32.h"   /* CANopenNodeSTM32 实例类型 */

/* 节点配置:默认节点 ID 与波特率(须与 OD / 物理接线一致) */
#define CO_NODE_ID        42    /* CANopen 节点 ID (1..127) */
#define CO_BAUDRATE_KBPS  250   /* CAN 波特率 (kbps) */

/**
 * @brief  初始化 CANopen 协议栈
 *         (在 BSP_Init 之后、主循环之前调用一次)
 * @retval 0 成功, 非0 失败
 */
int co_init(void);

/** @brief CANopen 1ms 定时器中断处理(实时):SYNC / PDO,须由 1ms 周期中断调用 */
void co_timer_isr(void);

/** @brief 获取 CANopenNode STM32 实例指针,供状态灯指示 / 访问节点使用 */
CANopenNodeSTM32* co_get_node(void);

/** @brief 业务初始化(LED 指示 + 应用变量维护),在 co_init() 成功之后调用一次 */
void co_business_init(void);

void CANopen_Init();
void CANopen_run();
#ifdef __cplusplus
}
#endif

#endif /* __CANOPEN_USR_H */
