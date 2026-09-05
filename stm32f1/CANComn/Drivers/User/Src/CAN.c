#include "CAN.h"
CAN_HandleTypeDef hcan1; // 全局CAN句柄

/**
 * @brief CAN1 初始化函数 (由CubeMX生成，可手动调整)
 */
void MX_CAN1_Init(void)
{
    hcan1.Instance = CAN1;
    hcan1.Init.Prescaler = 16;         // 36MHz/16/(1+5+3)=250kbps (原8为500k)
    hcan1.Init.Mode = CAN_MODE_NORMAL; // 正常模式，自测可用 CAN_MODE_LOOPBACK
    hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan1.Init.TimeSeg1 = CAN_BS1_5TQ;
    hcan1.Init.TimeSeg2 = CAN_BS2_3TQ;
    hcan1.Init.TimeTriggeredMode = DISABLE;
    hcan1.Init.AutoBusOff = DISABLE;
    hcan1.Init.AutoWakeUp = DISABLE;
    hcan1.Init.AutoRetransmission = DISABLE;
    hcan1.Init.ReceiveFifoLocked = DISABLE;
    hcan1.Init.TransmitFifoPriority = DISABLE;
    if (HAL_CAN_Init(&hcan1) != HAL_OK)
    {
        Error_Handler(); // 初始化错误处理
    }
}

/**
 * @brief 配置CAN过滤器（必须手动添加！）
 */
void CAN_Filter_Config(void)
{
    CAN_FilterTypeDef sFilterConfig = {0};

    sFilterConfig.FilterBank = 0;                      // 使用过滤器组0
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;  // 屏蔽位模式
    sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT; // 32位过滤器

    /* 配置为接收所有标准帧（示例） */
    sFilterConfig.FilterIdHigh = 0x0000;                   // 32位ID高16位
    sFilterConfig.FilterIdLow = 0x0000;                    // 32位ID低16位
    sFilterConfig.FilterMaskIdHigh = 0x0000;               // 屏蔽位高16位，全0表示不关心任何位
    sFilterConfig.FilterMaskIdLow = 0x0000;                // 屏蔽位低16位
    sFilterConfig.FilterFIFOAssignment = CAN_FILTER_FIFO0; // 关联到FIFO0
    sFilterConfig.FilterActivation = ENABLE;
    sFilterConfig.SlaveStartFilterBank = 14; // F103系列只有1个CAN，此参数无意义，填14即可

    if (HAL_CAN_ConfigFilter(&hcan1, &sFilterConfig) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief 启动CAN外设并激活中断通知（在主函数中调用）
 */
void CAN_Start(void)
{
    if (HAL_CAN_Start(&hcan1) != HAL_OK) // 启动CAN
    {
        Error_Handler();
    }
    // 激活FIFO0消息挂起中断，使能接收中断
    if (HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief CAN 报文发送函数（阻塞轮询方式）
 * @param hcan          CAN句柄
 * @param id            报文ID（标准帧11位，扩展帧29位）
 * @param data          数据指针
 * @param len           数据长度（0~8）
 * @param is_extended   是否为扩展帧（0：标准帧，1：扩展帧）
 * @retval HAL状态      HAL_OK 表示成功，其他表示失败
 */
HAL_StatusTypeDef CAN_SendMessage(CAN_HandleTypeDef *hcan,
                                  uint32_t id,
                                  uint8_t *data,
                                  uint8_t len,
                                  uint8_t is_extended)
{
    CAN_TxHeaderTypeDef tx_header = {0};
    uint32_t tx_mailbox = 0;   // 用于存储分配到的邮箱编号（0~2）
    uint32_t timeout = 0xFFFF; // 超时等待计数

    // 1. 配置发送帧头信息
    tx_header.StdId = id;                                  // 标准ID（若为扩展帧，此字段仍可填，但通常填低11位）
    tx_header.ExtId = id;                                  // 扩展ID（若为标准帧，此字段无效）
    tx_header.IDE = is_extended ? CAN_ID_EXT : CAN_ID_STD; // 帧类型
    tx_header.RTR = CAN_RTR_DATA;                          // 数据帧（若要发送远程帧则改为 CAN_RTR_REMOTE）
    tx_header.DLC = len;                                   // 数据长度 (0~8)
    tx_header.TransmitGlobalTime = DISABLE;                // 时间戳功能，一般关闭

    // 2. 尝试将报文添加到发送邮箱（非阻塞，但内部等待邮箱空闲的超时由用户控制）
    //    循环等待空闲邮箱，最多尝试 timeout 次
    while (HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0)
    {
        if (--timeout == 0)
        {
            return HAL_TIMEOUT; // 超时，所有邮箱都被占用
        }
    }

    // 3. 调用HAL库发送函数
    HAL_StatusTypeDef status = HAL_CAN_AddTxMessage(hcan, &tx_header, data, &tx_mailbox);

    // 4. (可选) 等待发送完成，超时机制
    if (status == HAL_OK)
    {
        uint32_t wait_time = 0xFFFF;
        // 等待该邮箱发送完成 (判断对应标志位)
        while (HAL_CAN_IsTxMessagePending(hcan, tx_mailbox))
        {
            if (--wait_time == 0)
            {
                return HAL_TIMEOUT;
            }
        }
    }

    return status;
}