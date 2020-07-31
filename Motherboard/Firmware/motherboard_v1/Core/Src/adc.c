#include "adc.h"
#include "drv_spi.h"
#include "platform.h"
#include <stdbool.h>
#include <stdint.h>

static void setup_pin_SYNC_ADC(void);
static void setup_pin_CONVST(void);

#define GPIO_SET_PIN(port, pin, x) port->BSRR = (x) ? pin : (pin << 16)

#define SET_PIN_CONVST_HIGH GPIO_SET_PIN(GPIOF, GPIO_PIN_6, 1)
#define SET_PIN_CONVST_LOW  GPIO_SET_PIN(GPIOF, GPIO_PIN_6, 0)

#define ADC_FRONTEND_GAIN (10.0)
#define ADC_FULL_SCALE    (1 << 15)
#define ADC_REF_VOLTAGE   (4.096)

#define ADC_BITS_TO_VOLTS(bits) ((float) ADC_REF_VOLTAGE * (float) bits / (float) ADC_FULL_SCALE)

void adc_init(void)
{
    setup_pin_SYNC_ADC();
    setup_pin_CONVST();
}

void adc_read_volts(float *out1, float *out2)
{
    // Get raw bits from ADC
    uint16_t data1;
    uint16_t data2;
    adc_read_raw_bitbang(&data1, &data2);

    // Convert raw bits to input voltage
    float vout1 = ADC_FRONTEND_GAIN * ADC_BITS_TO_VOLTS(data1);
    float vout2 = ADC_FRONTEND_GAIN * ADC_BITS_TO_VOLTS(data2);

    // Give user their data
    *out1 = vout1;
    *out2 = vout2;
}

void adc_read_raw_spi(uint16_t *out1, uint16_t *out2)
{
    // This function has been optimized for very fast
    // operation. It directly manipulates the SPI peripheral
    // registers to read in data from the ADCs.
    //
    // The ADC devices support a max of 400ksps. Looking at
    // the waveforms from this function, the CONVST line is
    // asserted for effectively 337kHz, approaching the limit.

    // Start ADC conversion
    SET_PIN_CONVST_HIGH;

    // Wait for ADC conversion to complete (per datasheet, >= 1300ns)
    //
    // Each i takes ~150 ns at 200 MHz clock and -O2,
    // so 1300 / ~150 = ~9 loops
    for (volatile int i = 0; i < 9; i++) {
    }

    // Read data bits from ADCs
    drv_spi_read_two_16bits(SPI5, out1, out2);

    // End conversion
    SET_PIN_CONVST_LOW;
}

static void setup_pin_CONVST(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    __HAL_RCC_GPIOF_CLK_ENABLE();

    // Configure GPIO pin Output Level
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_6, GPIO_PIN_RESET);

    // Configure GPIO pins
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
}

static void setup_pin_SYNC_ADC(void)
{
    // ADC Sync is a square wave input where every edge should
    // trigger a sampling event from the motherboard ADCs.
    //
    // These edges are aligned to the PWM carrier on the AMDC.

    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    __HAL_RCC_GPIOA_CLK_ENABLE();

    // Configure GPIO pin Output Level
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);

    // Configure GPIO pins
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // EXTI interrupt init
    HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(EXTI3_IRQn);
}

void EXTI3_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_3);

    asm("nop");
}
