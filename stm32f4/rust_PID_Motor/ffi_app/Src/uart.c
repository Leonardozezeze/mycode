#include "stm32f4xx.h"
#include "uart.h"

extern uint8_t rust_push_buffer(uint8_t rx_byte);
extern uint8_t rust_push_buffer4(uint8_t rx_byte);
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart4;

/* HAL 通过指针写入，不需要 volatile */
static uint8_t rx_byte;
static uint8_t rx_byte4;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        (void)rust_push_buffer(rx_byte);
        (void)HAL_UART_Receive_IT(&huart1, &rx_byte, 1); /* 重新武装 */
    }
    if (huart->Instance == UART4)
    {
        (void)rust_push_buffer4(rx_byte4);
        (void)HAL_UART_Receive_IT(&huart4, &rx_byte4, 1); /* 重新武装 */
    }
}
/* 启动第一次接收，在 board_drive_init() 里调用一次 */
void uart1_rx_start(void)
{
    (void)HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}
void uart1_write(const uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)data, len, HAL_MAX_DELAY);
}

/* UART4：使能 NVIC 中断并启动第一次接收，在 board_drive_init() 里调用一次 */
void uart4_rx_start(void)
{
    HAL_NVIC_SetPriority(UART4_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(UART4_IRQn);
    (void)HAL_UART_Receive_IT(&huart4, &rx_byte4, 1);
}
void uart4_write(const uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(&huart4, (uint8_t *)data, len, HAL_MAX_DELAY);
}
