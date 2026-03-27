#ifndef DRV_UART_H
#define DRV_UART_H

#include "platform.h"
#include <stdint.h>
#include <stdbool.h>

void drv_uart_init(void);

extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;

extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart5;

extern DMA_HandleTypeDef hdma_usart2_tx;
extern DMA_HandleTypeDef hdma_usart3_tx;

extern DMA_HandleTypeDef hdma_uart4_rx;
extern DMA_HandleTypeDef hdma_uart5_rx;

// 16-byte accumulator
// Layout: [UART4 pkt0-3 | UART5 pkt0-3][UART4 pkt4-7 | UART5 pkt4-7]
extern volatile uint16_t latest_valid_amds_samples[2][8];
extern volatile bool amds_samples_ready[2];
extern volatile uint8_t uart4_amds_sample_count;
extern volatile uint8_t uart5_amds_sample_count;


typedef enum {
    STATE_IDLE,
    STATE_GOT_HEADER,
} rx_state_t;

typedef struct {
    rx_state_t state;
    uint8_t header;
    uint8_t data[2];
    uint32_t read_index;
} uart_rx_tracker_t;

extern uart_rx_tracker_t tracker4;
extern uart_rx_tracker_t tracker5;

#define AMDS_RX_BUF_SIZE 256
extern uint8_t UART4_DMA_Pool[AMDS_RX_BUF_SIZE];
extern uint8_t UART5_DMA_Pool[AMDS_RX_BUF_SIZE];

// Define large buffers for outgoing data
#define TX_BUF_SIZE 512
extern uint8_t usart2_tx_ring[TX_BUF_SIZE];
extern uint8_t usart3_tx_ring[TX_BUF_SIZE];

extern uint32_t usart2_tx_write_idx;
extern uint32_t usart3_tx_write_idx;


void process_uart_fifo(uint8_t *pool, uart_rx_tracker_t *track, uint8_t uart_id);

void dma_send(uint8_t uart_id, uint8_t *data, uint8_t len);

static inline void drv_uart_putc_fast(USART_TypeDef *uart, uint8_t data)
{
    // Wait until UART is ready to accept next character
    while (!(uart->ISR & UART_FLAG_TXE)) {
        asm("nop");
    }

    // Load the TDR register to send a character
    uart->TDR = data;
}

static inline void drv_uart_putc_dma(USART_TypeDef *uart, uint8_t data)
{
    if (uart == USART2) {
        usart2_tx_ring[usart2_tx_write_idx] = data;
        usart2_tx_write_idx = (usart2_tx_write_idx + 1) % TX_BUF_SIZE;
    } else if (uart == USART3) {
        usart3_tx_ring[usart3_tx_write_idx] = data;
        usart3_tx_write_idx = (usart3_tx_write_idx + 1) % TX_BUF_SIZE;
    }
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
