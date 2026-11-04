#ifndef __TB6612_H
#define __TB6612_H

#include <stdint.h>

#define TB6612_PWM_MAX 1000 // 对应 ARR+1，CCR=TB6612_PWM_MAX 即 100% 占空比

/* ========================= TB6612 驱动（供 Rust FFI）=========================
 * 芯片级：只做寄存器写入和真值表译码；斜坡/软启动/堵转保护等策略
 * 全部在 Rust 侧（rust/src/peripheral/motor.rs），与 encoder 分层原则一致。
 *
 * 方向脚真值表：
 *   AIN1 AIN2 | 状态
 *     0    0  | 滑行（自由停）
 *     1    0  | 正转 CW
 *     0    1  | 反转 CCW
 *     1    1  | 短路刹车
 */

/* 启动 PWM + 使能 STBY + 置入安全状态（滑行、占空比 0） */
void tb6612_init(void);
/* 写占空比 CCR，超过 TB6612_PWM_MAX 自动钳位 */
void tb6612_set_pwm(uint16_t pwm);
/* 写方向脚，0 = 低电平，非 0 = 高电平 */
void tb6612_set_dir(uint8_t ain1, uint8_t ain2);
/* STBY 使能 / 失能（STBY 直连 3.3V 时为空操作） */
void tb6612_enable(void);
void tb6612_disable(void);
/* 带符号调速：正 = 正转，负 = 反转，幅值 = 占空比（自动钳位） */
void tb6612_set_speed(int16_t speed);

#endif
