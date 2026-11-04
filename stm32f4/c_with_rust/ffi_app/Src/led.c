#include "stm32f4xx.h"

/* 板载 LED：PF9 红 / PF10 绿，低电平点亮 */
#define RED_LED_PORT GPIOF
#define RED_LED_PIN GPIO_PIN_9
#define GREEN_LED_PORT GPIOF
#define GREEN_LED_PIN GPIO_PIN_10

void red_led_on(void)
{
    HAL_GPIO_WritePin(RED_LED_PORT, RED_LED_PIN, GPIO_PIN_RESET);
}

void red_led_off(void)
{
    HAL_GPIO_WritePin(RED_LED_PORT, RED_LED_PIN, GPIO_PIN_SET);
}

void red_led_toggle(void)
{
    HAL_GPIO_TogglePin(RED_LED_PORT, RED_LED_PIN);
}

void green_led_on(void)
{
    HAL_GPIO_WritePin(GREEN_LED_PORT, GREEN_LED_PIN, GPIO_PIN_RESET);
}

void green_led_off(void)
{
    HAL_GPIO_WritePin(GREEN_LED_PORT, GREEN_LED_PIN, GPIO_PIN_SET);
}

void green_led_toggle(void)
{
    HAL_GPIO_TogglePin(GREEN_LED_PORT, GREEN_LED_PIN);
}
