#ifndef __LED_H
#define __LED_H

/* 板载 LED：PF9 红 / PF10 绿，低电平点亮 */
void red_led_on(void);
void red_led_off(void);
void red_led_toggle(void);
void green_led_on(void);
void green_led_off(void);
void green_led_toggle(void);

#endif
