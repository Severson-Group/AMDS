#ifndef DRV_UART_H
#define DRV_UART_H

// BENCHMARK MODE FLAG for DMA
// #define BENCHMARK_MODE

#include "platform.h"
#include <stdbool.h>
#include <stdint.h>

void drv_uart_init(void);

extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;

extern UART_HandleTypeDef DAISY_RX1_UART;
extern UART_HandleTypeDef DAISY_RX2_UART;

extern DMA_HandleTypeDef hdma_uart4_rx;
extern DMA_HandleTypeDef hdma_uart5_rx;

extern DMA_HandleTypeDef hdma_uart6_rx;
extern DMA_HandleTypeDef hdma_uart1_rx;

typedef enum {
	STATE_IDLE, STATE_GOT_MSB, STATE_GOT_HEADER
} rx_state_t;

typedef struct {
	rx_state_t state;
	uint8_t header;
	uint8_t data[2];
	uint8_t read_index;
} uart_rx_tracker_t;

extern uart_rx_tracker_t tracker1;
extern uart_rx_tracker_t tracker2;

#define AMDS_RX_BUF_SIZE 256 // Must be 256 for uint8_t indexing and wrap-around logic to work correctly
extern uint8_t DAISY_RX1_Pool[AMDS_RX_BUF_SIZE];
extern uint8_t DAISY_RX2_Pool[AMDS_RX_BUF_SIZE];

#ifdef BENCHMARK_MODE
extern volatile uint8_t mock_dma_write_head;
#endif

// Declare the global flag so all .c files know it exists
extern volatile bool is_routing_active;

bool drv_uart_has_dma_data(void);

#define GPIO_TOGGLE_PIN(port, pin) ((port)->BSRR = ((port)->ODR & (pin)) ? ((pin) << 16) : (pin))

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
		tracker1.state = STATE_IDLE;
		tracker2.state = STATE_IDLE;

		// 2. Soft-flush the DMA buffers.
		// We advance our read pointers to exactly where the DMA hardware
		// is currently writing. All old, unprocessed bytes are instantly discarded.
#ifdef BENCHMARK_MODE
        tracker1.read_index = mock_dma_write_head;
        tracker2.read_index = mock_dma_write_head;
#else
		tracker1.read_index = (uint8_t) (AMDS_RX_BUF_SIZE
				- __HAL_DMA_GET_COUNTER(DAISY_RX1_UART.hdmarx));
		tracker2.read_index = (uint8_t) (AMDS_RX_BUF_SIZE
				- __HAL_DMA_GET_COUNTER(DAISY_RX2_UART.hdmarx));
#endif
	}
}

static inline void drv_uart_putc_fast(USART_TypeDef *uart, uint8_t data) {
	// Wait until UART is ready to accept next character
	while (!(uart->ISR & UART_FLAG_TXE)) {
		asm("nop");
	}

	// Load the TDR register to send a character
	uart->TDR = data;
}

static inline void drv_uart_wait_TC(USART_TypeDef *uart) {
	// ONLY USE THIS IF DISABLING THE UART OR GOING TO SLEEP!
	// This function waits for all data to be sent from the USART
	//    (it waits for both the TDR and the Shift Register to be
	//     completely empty)
	//
	// Do NOT USE THIS during normal continuous data transmission
	//       as it will add significant delays
	while (!(uart->ISR & UART_FLAG_TC)) {
		asm("nop");
	}
}

static inline void drv_uart_send_fast(USART_TypeDef *uart, uint8_t *data,
		uint16_t len) {
	while (len > 0) {
		drv_uart_putc_fast(uart, *data);
		data++;
		len--;
	}

	drv_uart_wait_TC(uart);
}

#endif // DRV_UART_H
