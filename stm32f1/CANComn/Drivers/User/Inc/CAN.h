#ifndef __CAN_H
#define __CAN_H
#include "bsp.h"

/* CAN1 全局句柄(定义于 CAN.c,供发送/过滤/中断使用) */
extern CAN_HandleTypeDef hcan1;

/* CAN1 初始化,由 BSP_Init 调用(CubeMX 风格) */
void MX_CAN1_Init(void);
/* 配置 CAN 过滤器(接收全部标准帧示例) */
void CAN_Filter_Config(void);
/* 启动 CAN 外设并开启 FIFO0 接收中断通知 */
void CAN_Start(void);
HAL_StatusTypeDef CAN_SendMessage(CAN_HandleTypeDef *hcan,
                                  uint32_t id,
                                  uint8_t *data,
                                  uint8_t len,
                                  uint8_t is_extended);
#endif