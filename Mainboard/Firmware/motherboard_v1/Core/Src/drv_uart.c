#include "drv_uart.h"
#include "tx.h"
#include "defines.h"
#include "drv_clock.h"
#include "platform.h"
#include <stdint.h>

static void MX_USART_UART_Init(UART_HandleTypeDef *huart, USART_TypeDef *handle);

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

DMA_HandleTypeDef hdma_usart2_tx;
DMA_HandleTypeDef hdma_usart3_tx;

// AMDS Daisy Chain RX Peripherals
UART_HandleTypeDef huart4;
UART_HandleTypeDef huart5;

DMA_HandleTypeDef hdma_uart4_rx;
DMA_HandleTypeDef hdma_uart5_rx;

uart_rx_tracker_t tracker4 = {0};
uart_rx_tracker_t tracker5 = {0};

uint8_t UART4_DMA_Pool[AMDS_RX_BUF_SIZE];
uint8_t UART5_DMA_Pool[AMDS_RX_BUF_SIZE];

// FBC Daisy Chain RX Peripherals
static UART_HandleTypeDef huart6;
static UART_HandleTypeDef huart1;

static DMA_HandleTypeDef hdma_uart6_rx;
static DMA_HandleTypeDef hdma_uart1_rx;

uart_rx_tracker_t tracker6 = {0};
uart_rx_tracker_t tracker1 = {0};

uint8_t UART6_DMA_Pool[AMDS_RX_BUF_SIZE];
uint8_t UART1_DMA_Pool[AMDS_RX_BUF_SIZE];

// Global flag to track if routing is actively occurring.
// Must be volatile so the compiler knows it can change inside an IRQ.
volatile bool is_routing_active = false;

#ifdef BENCHMARK_MODE
    volatile uint8_t mock_dma_write_head = 0;
    #define GET_W4() mock_dma_write_head
    #define GET_W5() mock_dma_write_head
#else
    // NDTR counts down, so the write head is (SIZE - NDTR).
    // Casting to uint8_t naturally handles the modulo wrap-around at 256.
    // AMDS_RX_BUF_SIZE MUST BE 256 FOR THIS MATH TO WORK PROPERLY!
    #define GET_W4() (uint8_t)(AMDS_RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart4.hdmarx))
    #define GET_W5() (uint8_t)(AMDS_RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart5.hdmarx))
#endif


void process_routing(void) {
    // 1. Load tracking state into local CPU registers for zero-wait-state access
    uint8_t r4 = tracker4.read_index;
    uint8_t r5 = tracker5.read_index;
    uint8_t s4 = tracker4.state;
    uint8_t s5 = tracker5.state;

    // 2. Read DMA hardware pointers ONCE at the start. 
    // NDTR counts down, so the write head is (SIZE - NDTR).
    // Casting to uint8_t naturally handles the modulo wrap-around at 256.
    uint8_t w4 = GET_W4();
    uint8_t w5 = GET_W5();

    // Process instantly as long as either buffer has data. No NOP delays!
    while ((r4 != w4) || (r5 != w5)) {
        
        // Calculate exactly how many bytes are sitting unread in the DMA buffer.
        // Because everything is cast to uint8_t, this math safely handles 
        // circular buffer wrap-around natively (e.g. w4=2, r4=254 -> avail=4)
        uint8_t avail4 = (uint8_t)(w4 - r4);
        uint8_t avail5 = (uint8_t)(w5 - r5);

        // =====================================================================
        // OPTIMIZATION 1: DUAL-STREAM FAST PATH (Perfect Interleaving)
        // =====================================================================
        // If BOTH streams have at least a full 3-byte packet, process them completely
        // interleaved to keep both hardware lines saturated simultaneously.
        while ((s4 == STATE_IDLE && avail4 >= 3) && (s5 == STATE_IDLE && avail5 >= 3)) {
            uint8_t h4 = UART4_DMA_Pool[r4];
            uint8_t h5 = UART5_DMA_Pool[r5];
            
            if (((h4 & 0xF0) == 0x90) && ((h5 & 0xF0) == 0x90)) {
                // Byte 1: Headers (Incremented)
                drv_uart_putc_fast(USART2, h4 + 4);
                drv_uart_putc_fast(USART3, h5 + 4);
                
                // Byte 2: MSB
                drv_uart_putc_fast(USART2, UART4_DMA_Pool[(uint8_t)(r4 + 1)]);
                drv_uart_putc_fast(USART3, UART5_DMA_Pool[(uint8_t)(r5 + 1)]);
                
                // Byte 3: LSB
                drv_uart_putc_fast(USART2, UART4_DMA_Pool[(uint8_t)(r4 + 2)]);
                drv_uart_putc_fast(USART3, UART5_DMA_Pool[(uint8_t)(r5 + 2)]);
                
                r4 += 3;
                r5 += 3;
                avail4 -= 3;
                avail5 -= 3;
            } else {
                break; // Misaligned or corrupted header, break to let the slow-path handle it
            }
        }

        // =====================================================================
        // OPTIMIZATION 2: SINGLE-STREAM FAST PATHS 
        // =====================================================================
        // If one UART receives data slightly faster than the other, process it.
        while (s4 == STATE_IDLE && avail4 >= 3) {
            uint8_t h4 = UART4_DMA_Pool[r4];
            if ((h4 & 0xF0) == 0x90) {
                drv_uart_putc_fast(USART2, h4 + 4);
                drv_uart_putc_fast(USART2, UART4_DMA_Pool[(uint8_t)(r4 + 1)]);
                drv_uart_putc_fast(USART2, UART4_DMA_Pool[(uint8_t)(r4 + 2)]);
                
                r4 += 3;
                avail4 -= 3;
            } else {
                break;
            }
        }

        while (s5 == STATE_IDLE && avail5 >= 3) {
            uint8_t h5 = UART5_DMA_Pool[r5];
            if ((h5 & 0xF0) == 0x90) {
                drv_uart_putc_fast(USART3, h5 + 4);
                drv_uart_putc_fast(USART3, UART5_DMA_Pool[(uint8_t)(r5 + 1)]);
                drv_uart_putc_fast(USART3, UART5_DMA_Pool[(uint8_t)(r5 + 2)]);
                
                r5 += 3;
                avail5 -= 3;
            } else {
                break;
            }
        }

        // =====================================================================
        // SLOW PATH: Fragmentation / State Recovery
        // =====================================================================
        // We only fall down here if a packet is fragmented across a DMA update
        // boundary or if data is corrupted. We can safely revert to the simple 
        // 1-byte-at-a-time logic.
        if (r4 != w4) {
            uint8_t b4 = UART4_DMA_Pool[r4++];
            if (s4 == STATE_IDLE) {
                if ((b4 & 0xF0) == 0x90) {
                    drv_uart_putc_fast(USART2, b4 + 4);
                    s4 = STATE_GOT_HEADER;
                }
            } else if (s4 == STATE_GOT_HEADER) {
                drv_uart_putc_fast(USART2, b4);
                s4 = STATE_GOT_MSB;
            } else { // STATE_GOT_MSB
                drv_uart_putc_fast(USART2, b4);
                s4 = STATE_IDLE;
            }
        }

        if (r5 != w5) {
            uint8_t b5 = UART5_DMA_Pool[r5++];
            if (s5 == STATE_IDLE) {
                if ((b5 & 0xF0) == 0x90) {
                    drv_uart_putc_fast(USART3, b5 + 4);
                    s5 = STATE_GOT_HEADER;
                }
            } else if (s5 == STATE_GOT_HEADER) {
                drv_uart_putc_fast(USART3, b5);
                s5 = STATE_GOT_MSB;
            } else { // STATE_GOT_MSB
                drv_uart_putc_fast(USART3, b5);
                s5 = STATE_IDLE;
            }
        }

        // Check if we caught up to our cached write pointers.
        // If so, re-sample the DMA registers to see if new data arrived 
        // while we were actively processing the previous bytes.
        if ((r4 == w4) && (r5 == w5)) {
            w4 = GET_W4();
            w5 = GET_W5();
        }
    }

    // 5. Store states back
    tracker4.read_index = r4;
    tracker5.read_index = r5;
    tracker4.state = s4;
    tracker5.state = s5;
}


void dma_queue(uint8_t uart_id, uint8_t *data, uint8_t len) {
    if (uart_id == 2) {
        for (int i = 0; i < len; i++) {
            uart2_dma_queue[u2_q_head] = data[i];
            u2_q_head = (u2_q_head + 1) % AMDS_RX_BUF_SIZE;
        }
    } else if (uart_id == 3) {
        for (int i = 0; i < len; i++) {
            uart3_dma_queue[u3_q_head] = data[i];
            u3_q_head = (u3_q_head + 1) % AMDS_RX_BUF_SIZE;
        }
    }
}

void UART4_IRQHandler(void)
{
    // Check for Parity, Overrun, Noise, or Frame errors
    if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_PE)  ||
        __HAL_UART_GET_FLAG(&huart4, UART_FLAG_ORE) ||
        __HAL_UART_GET_FLAG(&huart4, UART_FLAG_NE)  ||
        __HAL_UART_GET_FLAG(&huart4, UART_FLAG_FE))
    {
        // 1. Clear the error flags (Added UART_CLEAR_PEF)
        __HAL_UART_CLEAR_IT(&huart4, UART_CLEAR_PEF | UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_FEF);

        // 2. IMPORTANT: Re-enable DMA receiver request
        // The hardware/HAL drops this bit on error, halting the DMA stream.
        SET_BIT(huart4.Instance->CR3, USART_CR3_DMAR);

        return;
    }

    // Process normal RX/TX interrupts via the HAL
    HAL_UART_IRQHandler(&huart4);
}

void DMA1_Stream2_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_uart4_rx);
}

void UART5_IRQHandler(void)
{
	// Check for Overrun, Noise, or Frame errors
	if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_PE)  ||
		__HAL_UART_GET_FLAG(&huart5, UART_FLAG_ORE) ||
		__HAL_UART_GET_FLAG(&huart5, UART_FLAG_NE)  ||
		__HAL_UART_GET_FLAG(&huart5, UART_FLAG_FE))
	{
		// 1. Clear the error flags
		__HAL_UART_CLEAR_IT(&huart5, UART_CLEAR_PEF | UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_FEF);

		// 2. IMPORTANT: Re-enable DMA receiver request
		// Sometimes HAL disables this bit (DMAR) on error.
		SET_BIT(huart5.Instance->CR3, USART_CR3_DMAR);

		return;
	}
	HAL_UART_IRQHandler(&huart5);
}

void DMA1_Stream0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_uart5_rx);
}

// USART2 DMA and UART Interrupts
void DMA1_Stream6_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_usart2_tx);
}

void USART2_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart2);
}

// USART3 DMA and UART Interrupts
void DMA1_Stream3_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_usart3_tx);
}

void USART3_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart3);
}

void drv_uart_init(void)
{
    // Twiddle some bits in the RCC module to set USART2 and USART3 clock source.
    // By default, the source is PCLK1 (i.e. APB1 clock), but this is 4x slower than
    // the system clock. We can configure the clock tree to use SYSCLK to get
    // higher data throughput!
    //
    // This feature is not really documented that well... :(

    __HAL_RCC_USART2_CONFIG(RCC_USART2CLKSOURCE_SYSCLK);
    __HAL_RCC_USART3_CONFIG(RCC_USART3CLKSOURCE_SYSCLK);

    __HAL_RCC_UART4_CONFIG(RCC_UART4CLKSOURCE_SYSCLK);
	__HAL_RCC_UART5_CONFIG(RCC_UART5CLKSOURCE_SYSCLK);

	__HAL_RCC_USART1_CONFIG(RCC_USART1CLKSOURCE_SYSCLK);
	__HAL_RCC_USART6_CONFIG(RCC_USART6CLKSOURCE_SYSCLK);

    MX_USART_UART_Init(&huart2, USART2);
    MX_USART_UART_Init(&huart3, USART3);

    MX_USART_UART_Init(&huart4, UART4);
	MX_USART_UART_Init(&huart5, UART5);

    MX_USART_UART_Init(&huart1, USART1);
	MX_USART_UART_Init(&huart6, USART6);
}

static void MX_USART_UART_Init(UART_HandleTypeDef *huart, USART_TypeDef *handle)
{
    // Configure USART peripheral to run in transmit mode only, 8-bit data.
    //
    // Baud Rate: Each USART peripheral can be clocked from a variety of sources.
    // During uart_init() function, we set the clock tree mux such that USART2
    // and USART3 are now clocked by the system clock, which is configured to
    // 200 MHz via the PLL.
    //
    // If we configure our USART using oversampling of 8, we can get a max baud
    // rate of 200e6 / 8 = 25 Mbps
    uint32_t max_baudrate = SYSCLK_FREQ_HZ / 8; // 25 Mbps

    // Also, note that the AMDC FPGA is running at 200 MHz, so the fact that the
    // baud rate is an integer multiple of the FPGA clock is actually very nice!
    // This will make parsing data more robust on FPGA.

    huart->Instance = handle;
    huart->Init.BaudRate = max_baudrate;
    huart->Init.WordLength = UART_WORDLENGTH_9B;
    huart->Init.StopBits = UART_STOPBITS_2;
    huart->Init.Parity = UART_PARITY_ODD;

    if (huart->Instance == UART4 || huart->Instance == USART6) {
    	huart->Init.Mode = UART_MODE_RX;
    } else if (huart->Instance == UART5 || huart->Instance == USART1) {
    	huart->Init.Mode = UART_MODE_RX;
    } else {
    	huart->Init.Mode = UART_MODE_TX;
    }

    huart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart->Init.OverSampling = UART_OVERSAMPLING_8;
    huart->Init.OneBitSampling = UART_ONE_BIT_SAMPLE_ENABLE;
    huart->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(huart) != HAL_OK) {
        PANIC;
    }

    // Interrupt setup must come AFTER HAL_UART_Init()
	if (huart->Instance == UART4) {
    	NVIC_SetPriority(UART4_IRQn, 9);
		HAL_NVIC_EnableIRQ(UART4_IRQn);

		__HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF);
		__HAL_UART_FLUSH_DRREGISTER(huart);

		if (HAL_UART_Receive_DMA(&huart4, UART4_DMA_Pool, AMDS_RX_BUF_SIZE) != HAL_OK) {
		    PANIC;
		}
    } else if (huart->Instance == UART5) {
    	NVIC_SetPriority(UART5_IRQn, 9);
		HAL_NVIC_EnableIRQ(UART5_IRQn);

		__HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF);
		__HAL_UART_FLUSH_DRREGISTER(huart);

		if (HAL_UART_Receive_DMA(&huart5, UART5_DMA_Pool, AMDS_RX_BUF_SIZE) != HAL_OK) {
		    PANIC;
		}
	} else if (huart->Instance == USART6) {
    	NVIC_SetPriority(USART6_IRQn, 9);
		HAL_NVIC_EnableIRQ(USART6_IRQn);

		__HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF);
		__HAL_UART_FLUSH_DRREGISTER(huart);

		if (HAL_UART_Receive_DMA(&huart6, UART6_DMA_Pool, AMDS_RX_BUF_SIZE) != HAL_OK) {
		    PANIC;
		}
    } else if (huart->Instance == USART1) {
    	NVIC_SetPriority(USART1_IRQn, 9);
		HAL_NVIC_EnableIRQ(USART1_IRQn);

		__HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF);
		__HAL_UART_FLUSH_DRREGISTER(huart);

		if (HAL_UART_Receive_DMA(&huart1, UART1_DMA_Pool, AMDS_RX_BUF_SIZE) != HAL_OK) {
		    PANIC;
		}
	} else if (huart->Instance == USART2) {
    	NVIC_SetPriority(USART2_IRQn, 10);
		HAL_NVIC_EnableIRQ(USART2_IRQn);

		__HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF);
		__HAL_UART_FLUSH_DRREGISTER(huart);

    } else if (huart->Instance == USART3) {
    	NVIC_SetPriority(USART3_IRQn, 10);
		HAL_NVIC_EnableIRQ(USART3_IRQn);

		__HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF);
		__HAL_UART_FLUSH_DRREGISTER(huart);

	}
}

void HAL_UART_MspInit(UART_HandleTypeDef *uartHandle)
{

    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    if (uartHandle->Instance == USART2) {
        // USART2 clock enable
        __HAL_RCC_USART2_CLK_ENABLE();
        __HAL_RCC_DMA1_CLK_ENABLE();

        __HAL_RCC_GPIOA_CLK_ENABLE();
        // USART2 GPIO Configuration
        // PA2     ------> USART2_TX
        // PA3     ------> USART2_RX
        GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        // DMA config - check your device's DMA request mapping table
		// for the correct stream/channel for USART2_TX
        hdma_usart2_tx.Instance = DMA1_Stream6;
        hdma_usart2_tx.Instance->CR |= USART_CR3_DDRE;
        hdma_usart2_tx.Init.Channel = DMA_CHANNEL_4;
        hdma_usart2_tx.Init.Direction = DMA_MEMORY_TO_PERIPH; // Memory -> UART
        hdma_usart2_tx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_usart2_tx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_usart2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart2_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_usart2_tx.Init.Mode = DMA_NORMAL; // Keep it looping
        hdma_usart2_tx.Init.Priority = DMA_PRIORITY_LOW; // Let RX have higher priority

        if (HAL_DMA_Init(&hdma_usart2_tx) != HAL_OK) {
            PANIC;
        }
        __HAL_LINKDMA(uartHandle, hdmatx, hdma_usart2_tx);

		// DMA stream IRQ
		NVIC_SetPriority(DMA1_Stream6_IRQn, 7);  // higher priority than UART
		HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);
    }

    else if (uartHandle->Instance == USART3) {
        // USART3 clock enable
        __HAL_RCC_USART3_CLK_ENABLE();
        __HAL_RCC_DMA1_CLK_ENABLE();

        __HAL_RCC_GPIOB_CLK_ENABLE();
        // USART3 GPIO Configuration
        // PB10     ------> USART3_TX
        // PB11     ------> USART3_RX
        GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        // DMA config - check your device's DMA request mapping table
		// for the correct stream/channel for USART3_TX
		hdma_usart3_tx.Instance = DMA1_Stream3;
		hdma_usart3_tx.Instance->CR |= USART_CR3_DDRE;
		hdma_usart3_tx.Init.Channel = DMA_CHANNEL_4;
		hdma_usart3_tx.Init.Direction = DMA_MEMORY_TO_PERIPH; // Memory -> UART
		hdma_usart3_tx.Init.PeriphInc = DMA_PINC_DISABLE;
		hdma_usart3_tx.Init.MemInc = DMA_MINC_ENABLE;
		hdma_usart3_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
		hdma_usart3_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
		hdma_usart3_tx.Init.Mode = DMA_NORMAL; // Keep it looping
		hdma_usart3_tx.Init.Priority = DMA_PRIORITY_LOW; // Let RX have higher priority

		if (HAL_DMA_Init(&hdma_usart3_tx) != HAL_OK) {
			PANIC;
		}
		__HAL_LINKDMA(uartHandle, hdmatx, hdma_usart3_tx);

		// DMA stream IRQ
		NVIC_SetPriority(DMA1_Stream3_IRQn, 7);  // higher priority than UART
		HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
    }

    else if (uartHandle->Instance == UART4) {
		// USART3 clock enable
		__HAL_RCC_UART4_CLK_ENABLE();
		__HAL_RCC_DMA1_CLK_ENABLE();

		__HAL_RCC_GPIOD_CLK_ENABLE();
		// USART3 GPIO Configuration
		// PD0     ------> UART4_RX
		// PD1     ------> UART4_TX
		GPIO_InitStruct.Pin = GPIO_PIN_0;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF8_UART4;
		HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

		// DMA config - check your device's DMA request mapping table
		// for the correct stream/channel for UART4_RX
		hdma_uart4_rx.Instance = DMA1_Stream2;        // verify in datasheet
		hdma_uart4_rx.Init.Channel = DMA_CHANNEL_4; // HAL constant for your device
		hdma_uart4_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
		hdma_uart4_rx.Init.PeriphInc = DMA_PINC_DISABLE;  // RDR address stays fixed
		hdma_uart4_rx.Init.MemInc = DMA_MINC_ENABLE;      // buffer pointer increments
		hdma_uart4_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
		hdma_uart4_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
		hdma_uart4_rx.Init.Mode = DMA_CIRCULAR;         // or DMA_CIRCULAR (see note below)
		hdma_uart4_rx.Init.Priority = DMA_PRIORITY_HIGH;
		hdma_uart4_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

		if (HAL_DMA_Init(&hdma_uart4_rx) != HAL_OK) {
			PANIC;
		}

		// This links the DMA handle to the UART handle
		__HAL_LINKDMA(uartHandle, hdmarx, hdma_uart4_rx);

		// DMA stream IRQ
		NVIC_SetPriority(DMA1_Stream2_IRQn, 6);  // higher priority than UART
		HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);
	}

    else if (uartHandle->Instance == UART5) {
		// USART3 clock enable
		__HAL_RCC_UART5_CLK_ENABLE();
		__HAL_RCC_DMA1_CLK_ENABLE();

		__HAL_RCC_GPIOD_CLK_ENABLE();
		// USART3 GPIO Configuration
		// PD2      ------> UART5_RX
		GPIO_InitStruct.Pin = GPIO_PIN_2;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF8_UART5;
		HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

		// DMA config - check your device's DMA request mapping table
		// for the correct stream/channel for UART5_RX
		hdma_uart5_rx.Instance = DMA1_Stream0;        // verify in datasheet
		hdma_uart5_rx.Init.Channel = DMA_CHANNEL_4; // HAL constant for your device
		hdma_uart5_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
		hdma_uart5_rx.Init.PeriphInc = DMA_PINC_DISABLE;  // RDR address stays fixed
		hdma_uart5_rx.Init.MemInc = DMA_MINC_ENABLE;      // buffer pointer increments
		hdma_uart5_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
		hdma_uart5_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
		hdma_uart5_rx.Init.Mode = DMA_CIRCULAR;         // or DMA_CIRCULAR (see note below)
		hdma_uart5_rx.Init.Priority = DMA_PRIORITY_HIGH;
		hdma_uart5_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

		if (HAL_DMA_Init(&hdma_uart5_rx) != HAL_OK) {
			PANIC;
		}

		// This links the DMA handle to the UART handle
		__HAL_LINKDMA(uartHandle, hdmarx, hdma_uart5_rx);

		// DMA stream IRQ
		NVIC_SetPriority(DMA1_Stream0_IRQn, 6);  // higher priority than UART
		HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
	}

    else if (uartHandle->Instance == USART6) {
		// USART6 clock enable
		__HAL_RCC_USART6_CLK_ENABLE();
		__HAL_RCC_DMA2_CLK_ENABLE();

		__HAL_RCC_GPIOG_CLK_ENABLE();
		// USART3 GPIO Configuration
		// PG9     ------> UART4_RX
		GPIO_InitStruct.Pin = GPIO_PIN_9;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
		HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

		// DMA config - check your device's DMA request mapping table
		// for the correct stream/channel for USART6_RX pg 253 of reference manual
		hdma_uart6_rx.Instance = DMA2_Stream2;        // verify in reference manual
		hdma_uart6_rx.Init.Channel = DMA_CHANNEL_5; // verify in reference manual
		hdma_uart6_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
		hdma_uart6_rx.Init.PeriphInc = DMA_PINC_DISABLE;  // RDR address stays fixed
		hdma_uart6_rx.Init.MemInc = DMA_MINC_ENABLE;      // buffer pointer increments
		hdma_uart6_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
		hdma_uart6_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
		hdma_uart6_rx.Init.Mode = DMA_CIRCULAR;         // or DMA_CIRCULAR (see note below)
		hdma_uart6_rx.Init.Priority = DMA_PRIORITY_HIGH;
		hdma_uart6_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

		if (HAL_DMA_Init(&hdma_uart6_rx) != HAL_OK) {
			PANIC;
		}

		// This links the DMA handle to the UART handle
		__HAL_LINKDMA(uartHandle, hdmarx, hdma_uart6_rx);

		// DMA stream IRQ
		NVIC_SetPriority(DMA2_Stream2_IRQn, 6);  // higher priority than UART
		HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);
	}

	else if (uartHandle->Instance == USART1) {
		// USART1 clock enable
		__HAL_RCC_USART1_CLK_ENABLE();
		__HAL_RCC_DMA2_CLK_ENABLE();

		__HAL_RCC_GPIOA_CLK_ENABLE();
		// USART1 GPIO Configuration
		// PA10      ------> USART1_RX
		GPIO_InitStruct.Pin = GPIO_PIN_10;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

		// DMA config - check your device's DMA request mapping table
		// for the correct stream/channel for USART1_RX pg 253 of reference manual
		hdma_uart1_rx.Instance = DMA2_Stream5;        // verify in reference manual
		hdma_uart1_rx.Init.Channel = DMA_CHANNEL_4; // verify in reference manual
		hdma_uart1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
		hdma_uart1_rx.Init.PeriphInc = DMA_PINC_DISABLE;  // RDR address stays fixed
		hdma_uart1_rx.Init.MemInc = DMA_MINC_ENABLE;      // buffer pointer increments
		hdma_uart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
		hdma_uart1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
		hdma_uart1_rx.Init.Mode = DMA_CIRCULAR;         // or DMA_CIRCULAR (see note below)
		hdma_uart1_rx.Init.Priority = DMA_PRIORITY_HIGH;
		hdma_uart1_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

		if (HAL_DMA_Init(&hdma_uart1_rx) != HAL_OK) {
			PANIC;
		}

		// This links the DMA handle to the UART handle
		__HAL_LINKDMA(uartHandle, hdmarx, hdma_uart1_rx);

		// DMA stream IRQ
		NVIC_SetPriority(DMA2_Stream5_IRQn, 6);  // higher priority than UART
		HAL_NVIC_EnableIRQ(DMA2_Stream5_IRQn);
	}
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *uartHandle)
{

    if (uartHandle->Instance == USART2) {
        /* Peripheral clock disable */
        __HAL_RCC_USART2_CLK_DISABLE();

        /**USART2 GPIO Configuration
        PA2     ------> USART2_TX
        PA3     ------> USART2_RX
        */
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2 | GPIO_PIN_3);
    }

    else if (uartHandle->Instance == USART3) {
        /* Peripheral clock disable */
        __HAL_RCC_USART3_CLK_DISABLE();

        /**USART3 GPIO Configuration
        PB10     ------> USART3_TX
        PB11     ------> USART3_RX
        */
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10 | GPIO_PIN_11);
    }

    else if (uartHandle->Instance == UART4) {
		/* Peripheral clock disable */
		__HAL_RCC_UART4_CLK_DISABLE();

		/**USART3 GPIO Configuration
		PD0     ------> UART4_RX
		PD1     ------> UART4_TX
		*/
		HAL_GPIO_DeInit(GPIOD, GPIO_PIN_0);
	}

    else if (uartHandle->Instance == UART5) {
		/* Peripheral clock disable */
		__HAL_RCC_UART5_CLK_DISABLE();

		/**USART3 GPIO Configuration
		PD2      ------> UART5_RX
		*/
		HAL_GPIO_DeInit(GPIOD, GPIO_PIN_2);
	}

    else if (uartHandle->Instance == USART6) {
		/* Peripheral clock disable */
		__HAL_RCC_USART6_CLK_DISABLE();

		/**USART3 GPIO Configuration
		PG9     ------> USART6_RX
		PG10    ------> USART6_TX
		*/
		HAL_GPIO_DeInit(GPIOG, GPIO_PIN_9 | GPIO_PIN_10);
	}

	else if (uartHandle->Instance == USART1) {
		/* Peripheral clock disable */
		__HAL_RCC_USART1_CLK_DISABLE();

		/**USART3 GPIO Configuration
		PA9      ------> UART5_TX
		PA10     ------> UART5_RX
		*/
		HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);
	}
}
