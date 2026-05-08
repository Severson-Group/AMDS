#include "drv_uart.h"
#include "defines.h"
#include "drv_clock.h"
#include "platform.h"
#include <stdint.h>

static void MX_USART_UART_Init(UART_HandleTypeDef *huart, USART_TypeDef *handle);

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

DMA_HandleTypeDef hdma_uart4_rx;
DMA_HandleTypeDef hdma_uart5_rx;

DMA_HandleTypeDef hdma_uart6_rx;
DMA_HandleTypeDef hdma_uart1_rx;

// Daisy Chain RX Peripherals
UART_HandleTypeDef DAISY_RX1_UART;
UART_HandleTypeDef DAISY_RX2_UART;

uart_rx_tracker_t tracker1;
uart_rx_tracker_t tracker2;
uint8_t DAISY_RX1_Pool[AMDS_RX_BUF_SIZE];
uint8_t DAISY_RX2_Pool[AMDS_RX_BUF_SIZE];

// Global flag to track if routing is actively occurring.
// Must be volatile so the compiler knows it can change inside an IRQ.
volatile bool is_routing_active = false;

#ifdef BENCHMARK_MODE
    volatile uint8_t mock_dma_write_head = 0;
    #define GET_W1() mock_dma_write_head
    #define GET_W2() mock_dma_write_head
#else
    // NDTR counts down, so the write head is (SIZE - NDTR).
    // Casting to uint8_t naturally handles the modulo wrap-around at 256.
    // AMDS_RX_BUF_SIZE MUST BE 256 FOR THIS MATH TO WORK PROPERLY!
	#define GET_W1() (uint8_t)(AMDS_RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(DAISY_RX1_UART.hdmarx))
	#define GET_W2() (uint8_t)(AMDS_RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(DAISY_RX2_UART.hdmarx))
#endif

bool drv_uart_has_dma_data(void) {
	// Reading these 8-bit values is natively atomic, so it is safe to
	// evaluate them even if an interrupt is modifying them in the background.
	uint8_t w1 = GET_W1();
	uint8_t w2 = GET_W2();

	return (tracker1.read_index != w1) || (tracker2.read_index != w2);
}

void process_routing(void) {
	GPIO_TOGGLE_PIN(GPIOC, GPIO_PIN_6);
    // Load tracking state into local CPU registers
	uint8_t r1 = tracker1.read_index;
	uint8_t r2 = tracker2.read_index;
	uint8_t s1 = tracker1.state;
	uint8_t s2 = tracker2.state;

	uint8_t w1 = GET_W1();
	uint8_t w2 = GET_W2();

    // Process as long as either buffer has data
	while ((r1 != w1) || (r2 != w2)) {
        // Calculate how many bytes are sitting unread in the DMA buffer
        // Because everything is cast to uint8_t, this math safely handles 
        // circular buffer wrap-around natively (e.g. w4=2, r4=254 -> avail=4)
		uint8_t avail1 = (uint8_t)(w1 - r1);
		uint8_t avail2 = (uint8_t)(w2 - r2);


		if (avail1 < 3 || avail2 < 3) {
			// 1.3us timeout to let us receive enough data for dual-stream fast path
			uint32_t start_cycles = DWT->CYCCNT;

			// Calculate 1.3 microseconds in CPU cycles (integer math safe)
			uint32_t wait_cycles = (SystemCoreClock / 1000000) * 13 / 10;

			while ((avail1 >= 0 && avail1 <= 2) || (avail2 >= 0 && avail2 <= 2)) {
				w1 = GET_W1();
				w2 = GET_W2();

				avail1 = (uint8_t)(w1 - r1);
				avail2 = (uint8_t)(w2 - r2);

				// Break if we reach the 2us timeout
				if ((DWT->CYCCNT - start_cycles) > wait_cycles) {
					GPIO_TOGGLE_PIN(GPIOC, GPIO_PIN_7);
					break;
				}
			}
		}

        // =====================================================================
        // OPTIMIZATION 1: DUAL-STREAM FAST PATH (Perfect Interleaving)
        // =====================================================================
        // If both streams have at least a full 3-byte packet, process them completely
        // interleaved to keep both hardware lines saturated simultaneously.
        while ((s1 == STATE_IDLE && avail1 >= 3) && (s2 == STATE_IDLE && avail2 >= 3)) {
            uint8_t h1 = DAISY_RX1_Pool[r1];
            uint8_t h2 = DAISY_RX2_Pool[r2];
            
            if (((h1 & 0xF0) == 0x90) && ((h2 & 0xF0) == 0x90)) {
                // Byte 1: Headers (Incremented)
                drv_uart_putc_fast(USART2, h1 + 4);
                drv_uart_putc_fast(USART3, h2 + 4);
                
                // Byte 2: MSB
                drv_uart_putc_fast(USART2, DAISY_RX1_Pool[(uint8_t)(r1 + 1)]);
                drv_uart_putc_fast(USART3, DAISY_RX2_Pool[(uint8_t)(r2 + 1)]);
                
                // Byte 3: LSB
                drv_uart_putc_fast(USART2, DAISY_RX1_Pool[(uint8_t)(r1 + 2)]);
                drv_uart_putc_fast(USART3, DAISY_RX2_Pool[(uint8_t)(r2 + 2)]);

                r1 += 3;
                r2 += 3;
                avail1 -= 3;
                avail2 -= 3;

                if (avail1 < 3 || avail2 < 3) {
                	uint32_t start_cycles = DWT->CYCCNT;

					// Calculate 3 microseconds in CPU cycles (integer math safe)
					uint32_t wait_cycles = (SystemCoreClock / 1000000) * 3;

					while ((avail1 >= 1 && avail1 <= 2) || (avail2 >= 1 && avail2 <= 2)) {
						w1 = GET_W1();
						w2 = GET_W2();

						avail1 = (uint8_t)(w1 - r1);
						avail2 = (uint8_t)(w2 - r2);

						// Break if we reach the 2us timeout
						if ((DWT->CYCCNT - start_cycles) > wait_cycles) {
							GPIO_TOGGLE_PIN(GPIOC, GPIO_PIN_7);
							break;
						}
					}
                }
            } else {
                break; // Misaligned or corrupted header, break to let the slow-path handle it
            }
        }

        // =====================================================================
        // OPTIMIZATION 2: SINGLE-STREAM FAST PATHS 
        // =====================================================================
        // If one UART receives data slightly faster than the other, process it.
        while (s1 == STATE_IDLE && avail1 >= 3) {
            uint8_t h1 = DAISY_RX1_Pool[r1];
            if ((h1 & 0xF0) == 0x90) {
                drv_uart_putc_fast(USART2, h1 + 4);
                drv_uart_putc_fast(USART2, DAISY_RX1_Pool[(uint8_t)(r1 + 1)]);
                drv_uart_putc_fast(USART2, DAISY_RX1_Pool[(uint8_t)(r1 + 2)]);
                
                r1 += 3;
                avail1 -= 3;
            } else {
                break;
            }
        }

        while (s2 == STATE_IDLE && avail2 >= 3) {
            uint8_t h2 = DAISY_RX2_Pool[r2];
            if ((h2 & 0xF0) == 0x90) {
                drv_uart_putc_fast(USART3, h2 + 4);
                drv_uart_putc_fast(USART3, DAISY_RX2_Pool[(uint8_t)(r2 + 1)]);
                drv_uart_putc_fast(USART3, DAISY_RX2_Pool[(uint8_t)(r2 + 2)]);
                
                r2 += 3;
                avail2 -= 3;
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
        if (r1 != w1) {
            uint8_t b1 = DAISY_RX1_Pool[r1++];
            if (s1 == STATE_IDLE) {
                if ((b1 & 0xF0) == 0x90) {
                    drv_uart_putc_fast(USART2, b1 + 4);
                    s1 = STATE_GOT_HEADER;
                }
            } else if (s1 == STATE_GOT_HEADER) {
                drv_uart_putc_fast(USART2, b1);
                s1 = STATE_GOT_MSB;
            } else { // STATE_GOT_MSB
                drv_uart_putc_fast(USART2, b1);
                s1 = STATE_IDLE;
            }
        }

        if (r2 != w2) {
            uint8_t b2 = DAISY_RX2_Pool[r2++];
            if (s2 == STATE_IDLE) {
                if ((b2 & 0xF0) == 0x90) {
                    drv_uart_putc_fast(USART3, b2 + 4);
                    s2 = STATE_GOT_HEADER;
                }
            } else if (s2 == STATE_GOT_HEADER) {
                drv_uart_putc_fast(USART3, b2);
                s2 = STATE_GOT_MSB;
            } else { // STATE_GOT_MSB
                drv_uart_putc_fast(USART3, b2);
                s2 = STATE_IDLE;
            }
        }

        // Check if we caught up to our cached write pointers.
        // If so, re-sample the DMA registers to see if new data arrived 
        // while we were actively processing the previous bytes.
        if ((r1 == w1) && (r2 == w2)) {
            w1 = GET_W1();
            w2 = GET_W2();
        }
    }

	drv_uart_wait_TC(USART2);
	drv_uart_wait_TC(USART3);

    // Store states back
    tracker1.read_index = r1;
    tracker2.read_index = r2;
    tracker1.state = s1;
    tracker2.state = s2;
}

#if defined(TARGET_AMDS)
void UART4_IRQHandler(void)
{
    // Check for Parity, Overrun, Noise, or Frame errors
    if (__HAL_UART_GET_FLAG(&DAISY_RX1_UART, UART_FLAG_PE)  ||
        __HAL_UART_GET_FLAG(&DAISY_RX1_UART, UART_FLAG_ORE) ||
        __HAL_UART_GET_FLAG(&DAISY_RX1_UART, UART_FLAG_NE)  ||
        __HAL_UART_GET_FLAG(&DAISY_RX1_UART, UART_FLAG_FE))
    {
        // 1. Clear the error flags (Added UART_CLEAR_PEF)
        __HAL_UART_CLEAR_IT(&DAISY_RX1_UART, UART_CLEAR_PEF | UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_FEF);

        // 2. IMPORTANT: Re-enable DMA receiver request
        // The hardware/HAL drops this bit on error, halting the DMA stream.
        SET_BIT(DAISY_RX1_UART.Instance->CR3, USART_CR3_DMAR);

        return;
    }

    // Process normal RX/TX interrupts via the HAL
    HAL_UART_IRQHandler(&DAISY_RX1_UART);
}

void DMA1_Stream2_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_uart4_rx);
}

void UART5_IRQHandler(void)
{
	// Check for Overrun, Noise, or Frame errors
	if (__HAL_UART_GET_FLAG(&DAISY_RX2_UART, UART_FLAG_PE)  ||
		__HAL_UART_GET_FLAG(&DAISY_RX2_UART, UART_FLAG_ORE) ||
		__HAL_UART_GET_FLAG(&DAISY_RX2_UART, UART_FLAG_NE)  ||
		__HAL_UART_GET_FLAG(&DAISY_RX2_UART, UART_FLAG_FE))
	{
		// 1. Clear the error flags
		__HAL_UART_CLEAR_IT(&DAISY_RX2_UART, UART_CLEAR_PEF | UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_FEF);

		// 2. IMPORTANT: Re-enable DMA receiver request
		// Sometimes HAL disables this bit (DMAR) on error.
		SET_BIT(DAISY_RX2_UART.Instance->CR3, USART_CR3_DMAR);

		return;
	}
	HAL_UART_IRQHandler(&DAISY_RX2_UART);
}

void DMA1_Stream0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_uart5_rx);
}
#elif defined(TARGET_2S)
void USART6_IRQHandler(void)
{
    // Check for Parity, Overrun, Noise, or Frame errors
    if (__HAL_UART_GET_FLAG(&DAISY_RX1_UART, UART_FLAG_PE)  ||
        __HAL_UART_GET_FLAG(&DAISY_RX1_UART, UART_FLAG_ORE) ||
        __HAL_UART_GET_FLAG(&DAISY_RX1_UART, UART_FLAG_NE)  ||
        __HAL_UART_GET_FLAG(&DAISY_RX1_UART, UART_FLAG_FE))
    {
        // 1. Clear the error flags (Added UART_CLEAR_PEF)
        __HAL_UART_CLEAR_IT(&DAISY_RX1_UART, UART_CLEAR_PEF | UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_FEF);

        // 2. IMPORTANT: Re-enable DMA receiver request
        // The hardware/HAL drops this bit on error, halting the DMA stream.
        SET_BIT(DAISY_RX1_UART.Instance->CR3, USART_CR3_DMAR);

        return;
    }

    // Process normal RX/TX interrupts via the HAL
    HAL_UART_IRQHandler(&DAISY_RX1_UART);
}

void DMA2_Stream2_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_uart6_rx);
}

void USART1_IRQHandler(void)
{
	// Check for Overrun, Noise, or Frame errors
	if (__HAL_UART_GET_FLAG(&DAISY_RX2_UART, UART_FLAG_PE)  ||
		__HAL_UART_GET_FLAG(&DAISY_RX2_UART, UART_FLAG_ORE) ||
		__HAL_UART_GET_FLAG(&DAISY_RX2_UART, UART_FLAG_NE)  ||
		__HAL_UART_GET_FLAG(&DAISY_RX2_UART, UART_FLAG_FE))
	{
		// 1. Clear the error flags
		__HAL_UART_CLEAR_IT(&DAISY_RX2_UART, UART_CLEAR_PEF | UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_FEF);

		// 2. IMPORTANT: Re-enable DMA receiver request
		// Sometimes HAL disables this bit (DMAR) on error.
		SET_BIT(DAISY_RX2_UART.Instance->CR3, USART_CR3_DMAR);

		return;
	}
	HAL_UART_IRQHandler(&DAISY_RX2_UART);
}

void DMA2_Stream5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_uart1_rx);
}
#else
	#error "Please define a target board (TARGET_AMDS or TARGET_2S)!"
#endif

void USART2_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart2);
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
#if defined(TARGET_AMDS)
    __HAL_RCC_UART4_CONFIG(RCC_UART4CLKSOURCE_SYSCLK);
	__HAL_RCC_UART5_CONFIG(RCC_UART5CLKSOURCE_SYSCLK);
#elif defined(TARGET_2S)
	__HAL_RCC_USART6_CONFIG(RCC_USART6CLKSOURCE_SYSCLK);
	__HAL_RCC_USART1_CONFIG(RCC_USART1CLKSOURCE_SYSCLK);
#else
	#error "Please define a target board (TARGET_AMDS or TARGET_2S)!"
#endif

    MX_USART_UART_Init(&huart2, USART2);
    MX_USART_UART_Init(&huart3, USART3);

#if defined(TARGET_AMDS)
    MX_USART_UART_Init(&DAISY_RX1_UART, UART4);
	MX_USART_UART_Init(&DAISY_RX2_UART, UART5);
#elif defined(TARGET_2S)
    MX_USART_UART_Init(&DAISY_RX1_UART, USART6);
	MX_USART_UART_Init(&DAISY_RX2_UART, USART1);
#else
    #error "Please define a target board (TARGET_AMDS or TARGET_2S)!"
#endif
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

#if defined(TARGET_AMDS)
	if (huart->Instance == UART4) {
    	NVIC_SetPriority(UART4_IRQn, 9);
		HAL_NVIC_EnableIRQ(UART4_IRQn);

		__HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF);
		__HAL_UART_FLUSH_DRREGISTER(huart);

		if (HAL_UART_Receive_DMA(&DAISY_RX1_UART, DAISY_RX1_Pool, AMDS_RX_BUF_SIZE) != HAL_OK) {
		    PANIC;
		}
    }

	if (huart->Instance == UART5) {
    	NVIC_SetPriority(UART5_IRQn, 9);
		HAL_NVIC_EnableIRQ(UART5_IRQn);

		__HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF);
		__HAL_UART_FLUSH_DRREGISTER(huart);

		if (HAL_UART_Receive_DMA(&DAISY_RX2_UART, DAISY_RX2_Pool, AMDS_RX_BUF_SIZE) != HAL_OK) {
		    PANIC;
		}
	}
#elif defined(TARGET_2S)
	if (huart->Instance == USART6) {
    	NVIC_SetPriority(USART6_IRQn, 9);
		HAL_NVIC_EnableIRQ(USART6_IRQn);

		__HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF);
		__HAL_UART_FLUSH_DRREGISTER(huart);

		if (HAL_UART_Receive_DMA(&DAISY_RX1_UART, DAISY_RX1_Pool, AMDS_RX_BUF_SIZE) != HAL_OK) {
		    PANIC;
		}
    } else if (huart->Instance == USART1) {
    	NVIC_SetPriority(USART1_IRQn, 9);
		HAL_NVIC_EnableIRQ(USART1_IRQn);

		__HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF);
		__HAL_UART_FLUSH_DRREGISTER(huart);

		if (HAL_UART_Receive_DMA(&DAISY_RX2_UART, DAISY_RX2_Pool, AMDS_RX_BUF_SIZE) != HAL_OK) {
		    PANIC;
		}
	}
#else
	#error "Please define a target board (TARGET_AMDS or TARGET_2S)!"
#endif
	if (huart->Instance == USART2) {
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
    }

    else if (uartHandle->Instance == USART3) {
        // USART3 clock enable
        __HAL_RCC_USART3_CLK_ENABLE();

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
    }
#if defined(TARGET_AMDS)
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
#elif defined(TARGET_2S)
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
#else
	#error "Please define a target board (TARGET_AMDS or TARGET_2S)!"
#endif
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
#if defined(TARGET_AMDS)
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
#elif defined(TARGET_2S)
    else if (uartHandle->Instance == USART6) {
		/* Peripheral clock disable */
		__HAL_RCC_USART6_CLK_DISABLE();

		/**USART3 GPIO Configuration
		PG9     ------> USART6_RX
		PG14    ------> USART6_TX
		*/
		HAL_GPIO_DeInit(GPIOG, GPIO_PIN_9 | GPIO_PIN_14);
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
#else
	#error "Please define a target board (TARGET_AMDS or TARGET_2S)!"
#endif
}
