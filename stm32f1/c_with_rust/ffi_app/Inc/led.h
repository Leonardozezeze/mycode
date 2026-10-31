#ifndef __LED_H
#define __LED_H
/* 板载 LED（PC13，低电平点亮） */
void board_led_on(void);
void board_led_off(void);
void board_led_toggle(void);
#endif