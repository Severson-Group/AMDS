#include "adc.h"
#include "drv_spi.h"
#include "platform.h"
#include <stdbool.h>
#include <stdint.h>

static void setup_pin_SYNC_ADC(void);
static void setup_pin_CONVST(void);
static void setup_pin_SCLK(void);
static void setup_pin_SO(void);

#define GPIO_SET_PIN(port, pin, x) port->BSRR = (x) ? pin : (pin << 16)
#define GPIO_GET_PIN(port, pin)    ((port->IDR & pin) ? true : false)

static inline void set_pin_CONVST_high(void);
static inline void set_pin_CONVST_low(void);
static inline void set_pin_SCLK_high(void);
static inline void set_pin_SCLK_low(void);
static inline bool read_pin_SO(void);

// Configure ADC conversion wait type
// 1: Timed wait
// 0: Polled wait
#define ADC_CONVERSION_WAIT_TIMED (0)

#define ADC_FRONTEND_GAIN (10.0)
#define ADC_FULL_SCALE    (1 << 15)
#define ADC_REF_VOLTAGE   (4.096)

#define ADC_BITS_TO_VOLTS(bits) ((float) ADC_REF_VOLTAGE * (float) bits / (float) ADC_FULL_SCALE)

void adc_init(void)
{
    setup_pin_SYNC_ADC();
    setup_pin_CONVST();
    setup_pin_SCLK();
    setup_pin_SO();

    // SCLK needs to be idle HIGH
    set_pin_SCLK_high();
}

void adc_read_volts(float *out1, float *out2)
{
    // Get raw bits from ADC
    uint16_t data1;
    uint16_t data2;
    adc_read_raw(&data1, &data2);

    // Convert raw bits to input voltage
    float vout1 = ADC_FRONTEND_GAIN * ADC_BITS_TO_VOLTS(data1);
    float vout2 = ADC_FRONTEND_GAIN * ADC_BITS_TO_VOLTS(data2);

    // Give user their data
    *out1 = vout1;
    *out2 = vout2;
}

void adc_read_raw(uint16_t *out1, uint16_t *out2)
{
    // ADCs are wired up in Daisy-Chain Mode with Busy Indicator
    // See Figure 63 of datasheet...

    // Start ADC conversion...
    //
    // Must leave CONVST high the whole time
    // else ADC does not wake up!!!
    set_pin_CONVST_high();

#if ADC_CONVERSION_WAIT_TIMED
    // Wait for conversion to finish...
    // Per the datasheet, we need to wait >= 1300ns.
    // NOTE: we are not using the busy indicators!
    //
    // This loop is measured to wait ~3000ns
    for (volatile int i = 0; i < 700; i++) {
        asm("nop");
    }
#else
    // Wait for conversion to finish...
    // Per the datasheet, the data output is LOW during
    // the conversion. When it is done, it goes HIGH.

    // Wait for DOUT to become HIGH
    while (!read_pin_SO()) {
        asm("nop");
    }

    // Pause for a little bit. This is required to meet timing!
    for (volatile int i = 0; i < 15; i++) {
        asm("nop");
    }
#endif

    // Pull the data out of the ADC1...
    uint16_t data1 = 0;
    for (int i = 0; i < 16; i++) {
        // SCLK falling edges shift out next data bit...
        set_pin_SCLK_low();
        set_pin_SCLK_high();

        uint16_t bit = read_pin_SO() ? 1 : 0;
        data1 |= (bit << i);
    }

    // Pause for a little bit between ADCs so we can see waveforms better
    for (volatile int i = 0; i < 10; i++) {
        asm("nop");
    }

    // Pull the data out of the ADC2...
    uint16_t data2 = 0;
    for (int i = 0; i < 16; i++) {
        // SCLK falling edges shift out next data bit...
        set_pin_SCLK_low();
        set_pin_SCLK_high();

        uint16_t bit = read_pin_SO() ? 1 : 0;
        data2 |= (bit << i);
    }

#if 1
    // Send final clock...
    // This resets the ADC DOUT back to default state
    //
    // I believe this is optional, but recommended...? The ADC
    // will always output a HIGH bit on this clock cycle
    set_pin_SCLK_low();
    set_pin_SCLK_high();
#endif

    // Return CONVST to default state...
    set_pin_CONVST_low();

    // Give user their data
    *out1 = data1;
    *out2 = data2;
}

static inline void set_pin_CONVST_high(void)
{
    GPIO_SET_PIN(GPIOF, GPIO_PIN_6, 1);
}

static inline void set_pin_CONVST_low(void)
{
    GPIO_SET_PIN(GPIOF, GPIO_PIN_6, 0);
}

static inline void set_pin_SCLK_high(void)
{
    GPIO_SET_PIN(GPIOF, GPIO_PIN_7, 1);
}

static inline void set_pin_SCLK_low(void)
{
    GPIO_SET_PIN(GPIOF, GPIO_PIN_7, 0);
}

static inline bool read_pin_SO(void)
{
    return GPIO_GET_PIN(GPIOF, GPIO_PIN_7);
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

static void setup_pin_SCLK(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    __HAL_RCC_GPIOF_CLK_ENABLE();

    // Configure GPIO pin Output Level
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_7, GPIO_PIN_RESET);

    // Configure GPIO pins
    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
}

static void setup_pin_SO(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    __HAL_RCC_GPIOF_CLK_ENABLE();

    // Configure GPIO pin Output Level
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_8, GPIO_PIN_RESET);

    // Configure GPIO pins
    GPIO_InitStruct.Pin = GPIO_PIN_8;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
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
