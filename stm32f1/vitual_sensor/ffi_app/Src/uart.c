#include "stm32f1xx.h"
#include "uart.h"

extern uint8_t rust_push_buffer(uint8_t rx_byte);
extern uint8_t rust_push_buffer2(uint8_t rx_byte);
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

/* HAL 通过指针写入，不需要 volatile */
static uint8_t rx_byte;
static uint8_t rx_byte2;
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        (void)rust_push_buffer(rx_byte);
        (void)HAL_UART_Receive_IT(&huart1, &rx_byte, 1); /* 重新武装 */
    }
    if (huart->Instance == USART2)
    {
        (void)rust_push_buffer2(rx_byte2);
        (void)HAL_UART_Receive_IT(&huart2, &rx_byte2, 1); /* 重新武装 */
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

/* 启动第一次接收，在 board_drive_init() 里调用一次 */
void uart2_rx_start(void)
{
    (void)HAL_UART_Receive_IT(&huart2, &rx_byte2, 1);
}
void uart2_write(const uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)data, len, HAL_MAX_DELAY);
}
