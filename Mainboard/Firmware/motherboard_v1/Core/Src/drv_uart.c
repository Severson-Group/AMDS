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

UART_HandleTypeDef huart4;
UART_HandleTypeDef huart5;

DMA_HandleTypeDef hdma_uart4_rx;
DMA_HandleTypeDef hdma_uart5_rx;

uart_rx_tracker_t tracker4 = {0};
uart_rx_tracker_t tracker5 = {0};

uint8_t UART4_DMA_Pool[AMDS_RX_BUF_SIZE];
uint8_t UART5_DMA_Pool[AMDS_RX_BUF_SIZE];

void process_uart_fifo(uint8_t *pool, uart_rx_tracker_t *track, uint8_t uart_id) {
    // Calculate current DMA write position (NDTR counts down)
	UART_HandleTypeDef *huart;
	if (uart_id == 4) {
		huart = &huart4;
    } else {
    	huart = &huart5;
    }

	uint32_t dma_write_ptr = AMDS_RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart->hdmarx);

    while (track->read_index != dma_write_ptr) {
        uint8_t byte = pool[track->read_index];
        track->read_index = (track->read_index + 1) % AMDS_RX_BUF_SIZE;

        switch (track->state) {
            case STATE_IDLE:
                // Look for any valid header (0x90, 0x94, 0x98 ranges)
                if ((byte & 0xF0) == 0x90) {
                    track->header = byte;
                    track->state = STATE_GOT_HEADER;
                }
                break;

            case STATE_GOT_HEADER:
                track->data[0] = byte;

                byte = pool[track->read_index];
                track->read_index = (track->read_index + 1) % AMDS_RX_BUF_SIZE;
                track->data[1] = byte;

                // 1. Calculate the base index (0 to 3) within the 4-packet group
                uint8_t base_index = track->header & 0x03;

                // 2. Determine the set (0 for 0x90-0x93, 1 for 0x94-0x97)
                // Changed mask from 0x0C to 0x04, since we only care about bit 2.
                uint8_t sample_set = (track->header & 0x04) >> 2;

                // 3. Calculate the absolute global channel index (8 to 23)
                uint8_t global_index = 8 + (sample_set * 8) + (uart_id == 5 ? 4 : 0) + base_index;

//                // 3. Store the value logically rather than physically
//                latest_valid_amds_samples[bank][local_index] = value;
//
//                // 4. Update the ready flag for the specific bank
//                amds_samples_ready[bank] = true;

                if (global_index < 24) {
					tx_packets[global_index].header = track->header;
					tx_packets[global_index].msb = track->data[0];
					tx_packets[global_index].lsb = track->data[1];

					// Clear 'sent' before raising 'ready'
					packet_sent[global_index] = false;
					packet_ready[global_index] = true;
				}

                track->state = STATE_IDLE;
                break;
        }
    }
}

void dma_queue(uint8_t uart_id, uint8_t *data, uint8_t len) {

}

void UART4_IRQHandler(void)
{
    // Check for Overrun, Noise, or Frame errors
    if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_ORE) ||
        __HAL_UART_GET_FLAG(&huart4, UART_FLAG_NE)  ||
        __HAL_UART_GET_FLAG(&huart4, UART_FLAG_FE))
    {
        // 1. Clear the error flags
        __HAL_UART_CLEAR_IT(&huart4, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_FEF);

        // 2. IMPORTANT: Re-enable DMA receiver request
        // Sometimes HAL disables this bit (DMAR) on error.
        SET_BIT(huart4.Instance->CR3, USART_CR3_DMAR);
    }
    HAL_UART_IRQHandler(&huart4);
}

void DMA1_Stream2_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_uart4_rx);
}

void UART5_IRQHandler(void)
{
	// Check for Overrun, Noise, or Frame errors
	if (__HAL_UART_GET_FLAG(&huart5, UART_FLAG_ORE) ||
		__HAL_UART_GET_FLAG(&huart5, UART_FLAG_NE)  ||
		__HAL_UART_GET_FLAG(&huart5, UART_FLAG_FE))
	{
		// 1. Clear the error flags
		__HAL_UART_CLEAR_IT(&huart5, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_FEF);

		// 2. IMPORTANT: Re-enable DMA receiver request
		// Sometimes HAL disables this bit (DMAR) on error.
		SET_BIT(huart5.Instance->CR3, USART_CR3_DMAR);
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

    MX_USART_UART_Init(&huart2, USART2);
    MX_USART_UART_Init(&huart3, USART3);

    MX_USART_UART_Init(&huart4, UART4);
	MX_USART_UART_Init(&huart5, UART5);
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

    if (huart->Instance == UART4) {
    	huart->Init.Mode = UART_MODE_RX;
    } else if (huart->Instance == UART5) {
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
}
