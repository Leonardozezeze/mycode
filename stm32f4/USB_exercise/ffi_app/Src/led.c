#include"stm32f4xx.h"
/* 板载 LED：PF9，低电平点亮 */
#define BOARD_LED_PORT GPIOF
#define BOARD_LED_PIN GPIO_PIN_9
void board_led_on(void)
{
    HAL_GPIO_WritePin(BOARD_LED_PORT, BOARD_LED_PIN, GPIO_PIN_RESET);
}

void board_led_off(void)
{
    HAL_GPIO_WritePin(BOARD_LED_PORT, BOARD_LED_PIN, GPIO_PIN_SET);
}

void board_led_toggle(void)
{
    HAL_GPIO_TogglePin(BOARD_LED_PORT, BOARD_LED_PIN);
}