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
    drv_spi_init();
    drv_uart_init();
    drv_led_init();

    // Initialize the main modules
    adc_init();

    // Infinite loop (all real work is done in ISRs)
    uint8_t led = 0;
    uint32_t ledDelta = HAL_GetTick();

    // Disable the SysTick ISR
    //
    // The SysTick ISR causes jitter in the firmware operation,
    // and since this project does not use the SysTick features,
    // we do not need it to run during operation!
    //
    // Set bit 0 to 0
//    SysTick->CTRL &= 0xFFFFFFFE;
    
    while (1) {
    	if ((u2_q_tail != u2_q_head) || u3_q_tail != u3_q_head) {
			// Package all ready sets and transmit
			process_transmissions();
		}

    	if (tracker4.read_index != ( AMDS_RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart4.hdmarx))) { //update
    		process_uart_fifo(UART4_DMA_Pool, &tracker4, 4);
		}

    	if (tracker5.read_index != ( AMDS_RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(huart5.hdmarx))) { //update
			process_uart_fifo(UART5_DMA_Pool, &tracker5, 5);
		}

        if (HAL_GetTick() - ledDelta >= 250) {
        	ledDelta = HAL_GetTick();
        	drv_led_clear();
			drv_led_on(1 << led);
			drv_led_display();

			if (++led >= DRV_LED_NUM_TOTAL) {
				led = 0;
			}
        }
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
