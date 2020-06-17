#include "drv_led.h"
#include "defines.h"
#include "platform.h"
#include <stdint.h>

#define GPIO_SET_PIN(port, pin, x) port->BSRR = (x) ? pin : (pin << 16)

static void MX_LED_Init(void);

// Hold LED state as one bit per LED
// leds & 0x1 => DRV_LED1
// leds & 0x2 => DRV_LED2
// ...
static uint8_t leds;

void drv_led_init(void)
{
    // Initialize GPIO pins for LEDs
    MX_LED_Init();

    // Turn off all LEDs
    drv_led_clear();
    drv_led_display();
}

void drv_led_set(uint8_t pattern)
{
    leds = pattern;
}

void drv_led_clear(void)
{
    leds = 0;
}

void drv_led_on(uint8_t pattern)
{
    leds |= pattern;
}

void drv_led_off(uint8_t pattern)
{
    leds &= ~pattern;
}

void drv_led_toggle(uint8_t pattern)
{
    if (leds & pattern) {
        drv_led_off(pattern);
    } else {
        drv_led_on(pattern);
    }
}

static inline void drv_led_write(uint8_t x)
{
    GPIO_SET_PIN(GPIOD, GPIO_PIN_0, (x & (1 << 0)));
    GPIO_SET_PIN(GPIOD, GPIO_PIN_1, (x & (1 << 1)));
    GPIO_SET_PIN(GPIOD, GPIO_PIN_2, (x & (1 << 2)));
    GPIO_SET_PIN(GPIOD, GPIO_PIN_3, (x & (1 << 3)));
}

void drv_led_display(void)
{
    drv_led_write(leds);
}

static void MX_LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = { 0 };

    __HAL_RCC_GPIOD_CLK_ENABLE();

    // Configure GPIO pin Output Level
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_RESET);

    // Configure GPIO pins
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
}
