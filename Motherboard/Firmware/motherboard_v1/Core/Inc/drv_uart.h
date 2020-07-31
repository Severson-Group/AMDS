#ifndef DRV_UART_H
#define DRV_UART_H

#include <stdint.h>

void drv_uart_init(void);

void drv_uart_send(uint8_t amdc_uart, uint8_t *data, uint16_t len);

#endif // DRV_UART_H
