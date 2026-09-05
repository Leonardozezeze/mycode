/**
 * @file    canopen_usr.c
 * @brief   CANopen 协议栈集成模块(用户封装,含业务逻辑)
 *
 * 持有唯一的 CANopenNodeSTM32 实例,并在上面做协议栈初始化、
 * 非实时处理与 1ms 实时中断的封装。原 business.c 的 LED 状态指示
 * 与 OD 应用变量维护亦并入本模块,故本模块同时承载业务逻辑。
 * 协议栈本体及其 STM32 适配层 (Drivers/Middleware/CANopenNode)
 * 保持原样,本模块只负责接线。
 */

#include "canopen_usr.h"
#include "bsp.h"        /* htim4 / hcan / LED_GPIO_* / BSP_CAN_Init */
#include "OD.h"         /* OD_RAM 应用变量 */
#include <stdint.h>
#include <stdio.h>

/* 内部状态:唯一的 CANopenNode STM32 实例 */
static CANopenNodeSTM32 s_coNode;

/* 上一次业务周期刷新的时间戳 (ms) */
static uint32_t s_lastBusinessTick = 0;

/* 内部辅助函数 */
static void co_led_update(void);
static void co_od_vars_update(void);

void CANopen_Init()
{
    /* 启动横幅 */
    printf("\r\n=== CANopenNode STM32F103 ===\r\n");
    printf("Node ID: %d, Baudrate: %d kbps\r\n", CO_NODE_ID, CO_BAUDRATE_KBPS);

    /* 1. CANopen 协议栈 */
    if (co_init() != 0)
    {
        printf("ERROR: CANopen init failed!\r\n");
        Error_Handler();
    }

    /* 2. 业务逻辑 (LED 指示 + 应用变量) */
    co_business_init();

    printf("CANopen initialized successfully\r\n");
}
void CANopen_run()
{
    /* 1. CANopen 非实时处理 (SDO / EMCY / NMT / Heartbeat) */
    /* 处理 SDO / Emergency / NMT / Heartbeat 等非时间关键任务 */
    canopen_app_process();
    /* 2. 业务逻辑 (LED 状态 + 应用变量) */
    co_led_update();     /* CANopen 状态灯指示 */
    co_od_vars_update(); /* 应用变量维护(每秒一次) */
}
int co_init(void)
{
    /* 填充 CANopenNode STM32 配置 */
    s_coNode.desiredNodeID     = CO_NODE_ID;
    s_coNode.activeNodeID      = 0;
    s_coNode.baudrate          = CO_BAUDRATE_KBPS;
    s_coNode.timerHandle       = &htim4;        /* 1ms 定时器 */
    s_coNode.CANHandle         = &hcan;         /* CAN 控制器 */
    s_coNode.HWInitFunction    = BSP_CAN_Init;  /* 通信复位时由协议栈回调 */
    s_coNode.outStatusLEDGreen = 0;
    s_coNode.outStatusLEDRed   = 0;
    s_coNode.canOpenStack      = NULL;

    /* 启动 CANopen 协议栈 */
    return canopen_app_init(&s_coNode);
}

void co_timer_isr(void)
{
    /* 处理 SYNC / PDO 等时间关键任务,须在 1ms 周期中断中调用 */
    canopen_app_interrupt();
}

CANopenNodeSTM32* co_get_node(void)
{
    return &s_coNode;
}

/**
 * @brief  业务逻辑初始化,在 co_init() 成功之后调用一次
 *
 * LED 指示规则(板载 PC13,低电平点亮):
 *   快闪 (100ms) -> 协议栈报错 / Bus-Off (没接 CAN 总线时)
 *   慢闪 (500ms) -> 协议栈正常运行 (Operational)
 *   灭           -> 协议栈初始化中
 */
void co_business_init(void)
{
    s_lastBusinessTick = HAL_GetTick();
}

/* 按 CANopen 节点状态灯更新板载 LED 闪烁模式 */
static void co_led_update(void)
{
    static uint32_t lastToggle = 0;
    uint32_t now = HAL_GetTick();
    CANopenNodeSTM32 *node = co_get_node();

    if (node->outStatusLEDRed) {
        /* 错误:快闪 100ms */
        if (now - lastToggle >= 100) {
            HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);
            lastToggle = now;
        }
    } else if (node->outStatusLEDGreen) {
        /* 正常:慢闪 500ms */
        if (now - lastToggle >= 500) {
            HAL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);
            lastToggle = now;
        }
    } else {
        /* 初始化中:灭 */
        HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_SET);
    }
}

/*
 * 示例业务:每秒把 OD 应用变量 3 递增,满 10000 归零。
 * OD_RAM 结构体为协议栈直接可访问的全局实例,
 * 该变量可通过调试器 / gdb 观察其变化。
 * 之后若有业务数据需要映射到 PDO / SDO,在此维护即可。
 */
static void co_od_vars_update(void)
{
    uint32_t now = HAL_GetTick();

    if ((now - s_lastBusinessTick) >= 1000u) {
        s_lastBusinessTick = now;

        OD_RAM.x2002_applicationVariable3++;
        if (OD_RAM.x2002_applicationVariable3 >= 10000u) {
            OD_RAM.x2002_applicationVariable3 = 0u;
        }
    }
}
