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

typedef enum {
    STATE_IDLE,
    STATE_GOT_HEADER,
    STATE_GOT_MSB
} rx_state_t;

typedef struct {
    rx_state_t state;
    uint8_t header;
    uint8_t data[2];
    uint8_t read_index;
} uart_rx_tracker_t;

extern uart_rx_tracker_t tracker4;
extern uart_rx_tracker_t tracker5;

#define AMDS_RX_BUF_SIZE 256 // Must be 256 for uint8_t indexing and wrap-around logic to work correctly
extern uint8_t UART4_DMA_Pool[AMDS_RX_BUF_SIZE];
extern uint8_t UART5_DMA_Pool[AMDS_RX_BUF_SIZE];

extern volatile uint8_t uart2_dma_queue[AMDS_RX_BUF_SIZE];
extern volatile uint8_t uart3_dma_queue[AMDS_RX_BUF_SIZE];
extern volatile uint8_t uart2_dma_buffer[AMDS_RX_BUF_SIZE];
extern volatile uint8_t uart3_dma_buffer[AMDS_RX_BUF_SIZE];

// Queue tracking indices
extern volatile uint16_t u2_q_head;
extern volatile uint16_t u2_q_tail;
extern volatile uint16_t u3_q_head;
extern volatile uint16_t u3_q_tail;

// Declare the global flag so all .c files know it exists
extern volatile bool is_routing_active;

void process_uart_fifo(uint8_t *pool, uart_rx_tracker_t *track, uint8_t uart_id);
void dma_queue(uint8_t uart_id, uint8_t *data, uint8_t len);

void process_routing(void);

/**
 * Thread-safe, non-blocking wrapper for process_routing().
 * Uses an atomic try-lock to prevent reentrancy without 
 * stalling the CPU or blinding interrupts for too long.
 */
static inline void try_process_routing(void) {
    // 1. Enter brief critical section (approx. 3 CPU cycles)
    __disable_irq();
    
    // 2. Check if the lock is already claimed
    if (is_routing_active) {
        // Someone else is already routing. Safely abort.
        __enable_irq();
        return; 
    }
    
    // 3. Claim the lock
    is_routing_active = true;
    
    // 4. Exit critical section BEFORE the heavy lifting
    __enable_irq(); 

    // 5. Perform the actual routing with interrupts perfectly active
    process_routing();

    // 6. Release the lock when finished
    // (This single write is inherently atomic on a 32-bit ARM core, 
    // so we don't need to disable interrupts just to clear it).
    is_routing_active = false;
}

/**
 * Attempt to instantly reset the routing state machine and flush buffers.
 * To be called ONLY from the very beginning of EXTI3_IRQHandler.
 */
static inline void try_reset_routing_state(void) {
    // Because we are inside an IRQ, we preempted main(). 
    // We do NOT need to disable interrupts here to check the flag safely.
    if (!is_routing_active) {
        
        // 1. Reset state machines to gracefully await the next packet
        tracker4.state = STATE_IDLE;
        tracker5.state = STATE_IDLE;
        
        // 2. Soft-flush the DMA buffers.
        // We advance our read pointers to exactly where the DMA hardware 
        // is currently writing. All old, unprocessed bytes are instantly discarded.
        tracker4.read_index = (uint8_t)(AMDS_RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart4.hdmarx));
        tracker5.read_index = (uint8_t)(AMDS_RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart5.hdmarx));
    }
}

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

static inline void drv_uart_dma_send_fast(UART_HandleTypeDef *huart, uint8_t *data, uint16_t len)
{
    DMA_Stream_TypeDef *dma = (DMA_Stream_TypeDef *)huart->hdmatx->Instance;

    // 1. Disable the DMA channel
    dma->CR &= ~DMA_SxCR_EN;

    // 2. CRITICAL FIX: Wait for the hardware to actually halt.
    // Writing to address registers while EN is still high causes a silent failure.
    while ((dma->CR & DMA_SxCR_EN) != 0) {
        asm("nop");
    }

    // 3. Clear the DMA Transfer Complete and Half Transfer flags
    __HAL_DMA_CLEAR_FLAG(huart->hdmatx, __HAL_DMA_GET_TC_FLAG_INDEX(huart->hdmatx));
    __HAL_DMA_CLEAR_FLAG(huart->hdmatx, __HAL_DMA_GET_HT_FLAG_INDEX(huart->hdmatx));

    // 4. CRITICAL FIX: Tell the DMA exactly *where* to push the bytes.
    // It must point directly to the UART's Transmit Data Register.
    dma->PAR = (uint32_t)&huart->Instance->TDR;

    // 5. Load the Memory Address and Length registers
    dma->M0AR = (uint32_t)data;
    dma->NDTR = len;

    // 6. Clear UART Transmission Complete flag to ensure it's ready for a fresh burst
    huart->Instance->ICR = USART_ICR_TCCF;

    // 7. Fire!
    dma->CR |= DMA_SxCR_EN;
}

#endif // DRV_UART_H
