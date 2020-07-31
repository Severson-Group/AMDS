#include "drv_uart.h"
#include "defines.h"
#include "drv_clock.h"
#include "platform.h"
#include <stdint.h>

static void MX_USART_UART_Init(UART_HandleTypeDef *huart, USART_TypeDef *handle);

static UART_HandleTypeDef huart2;
static UART_HandleTypeDef huart3;

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

    MX_USART_UART_Init(&huart2, USART2);
    MX_USART_UART_Init(&huart3, USART3);
}

void drv_uart_send(uint8_t amdc_uart, uint8_t *data, uint16_t len)
{
    UART_HandleTypeDef *handle = NULL;

    switch (amdc_uart) {
    case 1:
        handle = &huart2;
        break;
    case 2:
        handle = &huart3;
        break;
    default:
        while (1) {
            asm("nop");
        }
    }

    HAL_StatusTypeDef err;
    err = HAL_UART_Transmit(handle, data, len, HAL_MAX_DELAY);

    if (err != HAL_OK) {
        while (1) {
            asm("nop");
        }
    }
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
    huart->Init.WordLength = UART_WORDLENGTH_8B;
    huart->Init.StopBits = UART_STOPBITS_1;
    huart->Init.Parity = UART_PARITY_NONE;
    huart->Init.Mode = UART_MODE_TX;
    huart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart->Init.OverSampling = UART_OVERSAMPLING_8;
    huart->Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(huart) != HAL_OK) {
        PANIC;
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
}
