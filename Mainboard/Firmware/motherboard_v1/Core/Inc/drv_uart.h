#ifndef DRV_UART_H
#define DRV_UART_H

#include "platform.h"
#include <stdint.h>

void drv_uart_init(void);

static inline void drv_uart_putc_fast(USART_TypeDef *uart, uint8_t data)
{
    // Wait until UART is ready to accept next character
    while (!(uart->ISR & UART_FLAG_TXE)) {
        asm("nop");
    }

    // Load the TDR register to send a character
    uart->TDR = data;
}

static inline void drv_uart_wait_TC(USART_TypeDef *uart)
{
    // After done sending characters, must wait for TC flag!!
    while (!(uart->ISR & UART_FLAG_TC)) {
        asm("nop");
    }
}

static inline void drv_uart_send_fast(USART_TypeDef *uart, uint8_t *data, uint16_t len)
{
    while (len > 0) {
        drv_uart_putc_fast(uart, *data);
        data++;
        len--;
    }

    drv_uart_wait_TC(uart);
}

#endif // DRV_UART_H
