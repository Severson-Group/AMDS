#include "drv_uart.h"
#include "defines.h"
#include "drv_clock.h"
#include "queue.h"
#include "platform.h"
#include <stdint.h>

static void MX_USART_UART_Init(UART_HandleTypeDef *huart, USART_TypeDef *handle);

static UART_HandleTypeDef huart2;
static UART_HandleTypeDef huart3;

static UART_HandleTypeDef huart4;
static UART_HandleTypeDef huart5;

// 3-byte packet buffers for each UART
#define PACKET_SIZE      3
#define PACKETS_PER_UART 4
#define TOTAL_PACKETS    (PACKETS_PER_UART * 2)   // 8 packets, 24 bytes total

static uint8_t UART4_RxBuf[PACKET_SIZE];
static uint8_t UART5_RxBuf[PACKET_SIZE];

// Shared 24-byte accumulator
// Layout: [UART4 pkt0..3 | UART5 pkt0..3]
static uint8_t shared_samples[TOTAL_PACKETS * PACKET_SIZE];
static uint8_t uart4_packet_count = 0;
static uint8_t uart5_packet_count = 0;

static void try_enqueue(void)
{
    if (uart4_packet_count == PACKETS_PER_UART &&
        uart5_packet_count == PACKETS_PER_UART)
    {
        amds_item_t item;
        for (int i = 0; i < sizeof(shared_samples); i++) {
            item.samples[i] = shared_samples[i];
        }
        queue_enqueue_from_isr(&sensor_queue, &item, current_id);

        // Reset for next set
        uart4_packet_count = 0;
        uart5_packet_count = 0;
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == UART4) {
        if (uart4_packet_count < PACKETS_PER_UART) {
            uint8_t offset = uart4_packet_count * PACKET_SIZE;  // first 12 bytes
            for (int i = 0; i < PACKET_SIZE; i++) {
                shared_samples[offset + i] = UART4_RxBuf[i];
            }
            uart4_packet_count++;
        }
        HAL_UART_Receive_IT(huart, UART4_RxBuf, PACKET_SIZE);
        try_enqueue();

    } else if (huart->Instance == UART5) {
        if (uart5_packet_count < PACKETS_PER_UART) {
            uint8_t offset = (PACKETS_PER_UART * PACKET_SIZE) + uart5_packet_count * PACKET_SIZE;  // last 12 bytes
            for (int i = 0; i < PACKET_SIZE; i++) {
                shared_samples[offset + i] = UART5_RxBuf[i];
            }
            uart5_packet_count++;
        }
        HAL_UART_Receive_IT(huart, UART5_RxBuf, PACKET_SIZE);
        try_enqueue();
    }
}

void UART4_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart4);
}

void UART5_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart5);
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
    	huart->Init.Mode = UART_MODE_TX_RX;
    } else if (huart->Instance == UART5) {
    	huart->Init.Mode = UART_MODE_RX;
    } else {
    	huart->Init.Mode = UART_MODE_TX;
    }

    huart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart->Init.OverSampling = UART_OVERSAMPLING_8;
    huart->Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(huart) != HAL_OK) {
        PANIC;
    }

    // Interrupt setup must come AFTER HAL_UART_Init()
	if (huart->Instance == UART4) {
		NVIC_SetPriority(UART4_IRQn, 11);
		HAL_NVIC_EnableIRQ(UART4_IRQn);

		__HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF);
		__HAL_UART_FLUSH_DRREGISTER(huart);

		if (HAL_UART_Receive_IT(huart, UART4_RxBuf, PACKET_SIZE) != HAL_OK) {
			PANIC;
		}
	} else if (huart->Instance == UART5) {
		NVIC_SetPriority(UART5_IRQn, 11);
		HAL_NVIC_EnableIRQ(UART5_IRQn);

		__HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF);
		__HAL_UART_FLUSH_DRREGISTER(huart);

		if (HAL_UART_Receive_IT(huart, UART5_RxBuf, PACKET_SIZE) != HAL_OK) {
			PANIC;
		}
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

    else if (uartHandle->Instance == UART4) {
		// USART3 clock enable
		__HAL_RCC_UART4_CLK_ENABLE();

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
	}

    else if (uartHandle->Instance == UART5) {
		// USART3 clock enable
		__HAL_RCC_UART5_CLK_ENABLE();

		__HAL_RCC_GPIOD_CLK_ENABLE();
		// USART3 GPIO Configuration
		// PD2      ------> UART5_RX
		GPIO_InitStruct.Pin = GPIO_PIN_2;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF8_UART5;
		HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
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
