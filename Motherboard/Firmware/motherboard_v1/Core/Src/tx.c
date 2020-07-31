#include "tx.h"
#include "adc.h"
#include "drv_uart.h"
#include "platform.h"

static void setup_pin_SYNC_TX(void);

void tx_init(void)
{
    // Set up pin which triggers ISR
    setup_pin_SYNC_TX();
}

// Called every edge on SYNC_TX pin.
//
// AMDC uses this signal to tell motherboard when
// to send ADC sample data back to AMDC.
void EXTI15_10_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_11);

    // TODO: testing this here for now
    // float volts1;
    // float volts2;
    // adc_read_volts(&volts1, &volts2);

    uint16_t bits1;
    uint16_t bits2;
    adc_read_raw_spi(&bits1, &bits2);

    uint16_t data[2];
    data[0] = bits1;
    data[1] = bits2;
    drv_uart_send(1, &data[0], 4);
}

static void setup_pin_SYNC_TX(void)
{
    // TX Sync is a square wave input where every edge should
    // trigger the transmission of the motherboard ADC samples
    // back to the AMDC.
    //
    // These edges are generated by the user's code on the AMDC,
    // so there are no guarantees on timing or frequency of transmission.

    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    __HAL_RCC_GPIOB_CLK_ENABLE();

    // Configure GPIO pin Output Level
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET);

    // Configure GPIO pins
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // EXTI interrupt init
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}
