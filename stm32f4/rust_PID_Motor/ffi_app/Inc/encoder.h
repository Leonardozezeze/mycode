#ifndef __ENCODER_H
#define __ENCODER_H
#include <stdint.h>

/* ========================= 编码器参数 =========================
 * 电机码盘 11 线，TIM4 用 TI12 模式（4 倍频），减速比 21.3：
 * 输出轴一圈计数 = 11 * 4 * 21.3 = 937.2（小数，禁止整数化）
 */
// #define ENCODER_PPR 11           // 编码器线数(电机轴每圈脉冲数)
// #define ENCODER_MULTIPLIER 4     // 4倍频(TI1 and TI2 模式)
// #define ENCODER_GEAR_RATIO 21.3f // 减速比

/* 计数方向：若读数与电机正转方向相反，改成 -1 */
#define ENCODER_DIR (+1)

/* 启动编码器计数（HAL_TIM_Encoder_Start），在 board_drive_init() 里调用一次 */
void encoder_init(void);
/* 读取当前 16 位计数，已按 ENCODER_DIR 转成有符号 */
int16_t encoder_get_count(void);
/* 计数清零 */
void encoder_reset(void);

#endif
