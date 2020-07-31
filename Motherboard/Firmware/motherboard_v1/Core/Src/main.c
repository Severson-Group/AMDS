// Drivers
#include "drv_clock.h"
#include "drv_gpio.h"
#include "drv_i2c.h"
#include "drv_led.h"
#include "drv_spi.h"
#include "drv_uart.h"
#include "platform.h"

// Modules
#include "adc.h"
#include "tx.h"

int main(void)
{
    // Reset of all peripherals, Initializes the Flash interface and the Systick
    HAL_Init();

    // Configure the system clock
    drv_clock_init();

    // Initialize peripherals
    //    drv_i2c_init();
    //    drv_gpio_init(); // TODO(NP): update the GPIO driver, not everything in there is actually a GPIO
    drv_spi_init();
    drv_uart_init();
    drv_led_init();

    adc_init();
    tx_init();

    // Infinite loop
    uint8_t led = 0;

    while (1) {
        drv_led_clear();
        drv_led_on(1 << led);
        drv_led_display();

        if (++led >= DRV_LED_NUM_TOTAL) {
            led = 0;
        }

        volatile int i;
        for (i = 0; i < 10000000; i++) {
            asm("nop");
        }

        // uint16_t data = drv_spi5_rx();
        asm("nop");
    }
}

void HAL_MspInit(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}
