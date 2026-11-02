#ifndef __UART_H
#define __UART_H

#include <stdint.h>

void uart1_rx_start(void);
void uart1_write(const uint8_t *data, uint16_t len);
#endif
