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

int main(void) {
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

	uint8_t led = 0;

	// Use DWT cycle counter instead of HAL_GetTick()
	uint32_t ledDelta = DWT->CYCCNT;

	// Calculate how many CPU cycles are in 250ms.
	uint32_t cyclesPer250ms = SystemCoreClock / 4;

	// Disable the SysTick ISR
	//
	// The SysTick ISR causes jitter in the firmware operation,
	// and since this project does not use the SysTick features,
	// we do not need it to run during operation!
	//
	// Set bit 0 to 0
	SysTick->CTRL &= 0xFFFFFFFE;

	// Enable the Cortex-M7 DWT Cycle Counter for hardware delays
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->LAR = 0xC5ACCE55;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

	// Infinite loop (all real work is done in ISRs)
	while (1) {

		// Handle DMA data from UARTs and route to correct destination.
		// Only attempt to grab the lock and route data if there
		// is actually data waiting in the DMA buffers to help the
		// external IRQ retain higher priority access to process_routing
		// IMPORTANT NOTE: the goal is to handle all data in the
		// interrupts. This pathway to process_routing() is being provided
		// as a fail safe. If it is regularly being used, consider this a
		// warning sign of a broader system problem as it will cause slow
		// link speeds.
		//if (drv_uart_has_dma_data())
		//	try_process_routing(); // This try function is thread safe

		// Handle LEDs using hardware cycle counts
		if (DWT->CYCCNT - ledDelta >= cyclesPer250ms) {
			ledDelta = DWT->CYCCNT;
			drv_led_clear();

			drv_led_on(1 << led);
			drv_led_display();

			if (++led >= DRV_LED_NUM_TOTAL) {
				led = 0;
			}
		}
	}
}

void HAL_MspInit(void) {
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_RCC_SYSCFG_CLK_ENABLE();
}

void SysTick_Handler(void) {
	HAL_IncTick();
}
