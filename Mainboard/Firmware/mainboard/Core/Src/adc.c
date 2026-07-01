#include "adc.h"
#include "drv_spi.h"
#include "drv_uart.h"
#include "platform.h"
#include <stdbool.h>
#include <stdint.h>

static void setup_pin_SYNC_ADC(void);
static void setup_pin_CONVST(void);

// clang-format off

#define NOP1   asm("nop")
#define NOP2   NOP1;NOP1
#define NOP4   NOP2;NOP2
#define NOP8   NOP4;NOP4
#define NOP16  NOP8;NOP8
#define NOP32  NOP16;NOP16
#define NOP64  NOP32;NOP32
#define NOP128 NOP64;NOP64
#define NOP256 NOP128;NOP128

// clang-format on

#define GPIO_SET_PIN(port, pin, x) port->BSRR = (x) ? pin : (pin << 16)
#define GPIO_TOGGLE_PIN(port, pin) ((port)->BSRR = ((port)->ODR & (pin)) ? ((pin) << 16) : (pin))

#define SET_PIN_CONVST12_HIGH GPIO_SET_PIN(GPIOE, GPIO_PIN_10, 1)
#define SET_PIN_CONVST34_HIGH GPIO_SET_PIN(GPIOE, GPIO_PIN_11, 1)
#define SET_PIN_CONVST56_HIGH GPIO_SET_PIN(GPIOF, GPIO_PIN_6, 1)
#define SET_PIN_CONVST78_HIGH GPIO_SET_PIN(GPIOG, GPIO_PIN_8, 1)

#define SET_PIN_CONVST12_LOW GPIO_SET_PIN(GPIOE, GPIO_PIN_10, 0)
#define SET_PIN_CONVST34_LOW GPIO_SET_PIN(GPIOE, GPIO_PIN_11, 0)
#define SET_PIN_CONVST56_LOW GPIO_SET_PIN(GPIOF, GPIO_PIN_6, 0)
#define SET_PIN_CONVST78_LOW GPIO_SET_PIN(GPIOG, GPIO_PIN_8, 0)

#define ADC_FULL_SCALE    (1 << 15)
#define ADC_REF_VOLTAGE   (4.096)
#define ADC_VOLTS_PER_BIT ((float) ADC_REF_VOLTAGE / (float) ADC_FULL_SCALE)

#define ADC_BITS_TO_VOLTS(bits) (ADC_VOLTS_PER_BIT * (float) bits)

#define SENSOR_MASK_S1 0b00000001
#define SENSOR_MASK_S2 0b00000010
#define SENSOR_MASK_S3 0b00000100
#define SENSOR_MASK_S4 0b00001000
#define SENSOR_MASK_S5 0b00010000
#define SENSOR_MASK_S6 0b00100000
#define SENSOR_MASK_S7 0b01000000
#define SENSOR_MASK_S8 0b10000000

const uint8_t SENSOR_MASK_LUT[8] = { SENSOR_MASK_S1, SENSOR_MASK_S2,
SENSOR_MASK_S3,
SENSOR_MASK_S4, SENSOR_MASK_S5, SENSOR_MASK_S6, SENSOR_MASK_S7,
SENSOR_MASK_S8 };

// Global bitmask: 1 = Active, 0 = Inactive.
// For example: 0b00001111 (0x0F) means channels 1-4 are active, 5-8 are disabled.
#if defined(TARGET_AMDS)
const static uint8_t active_sensor_mask = 0x11;
#elif defined(TARGET_2S)
const static uint8_t active_sensor_mask = 0x11;
#else
#error "Please define a target board (TARGET_AMDS or TARGET_2S)!"
#endif

uint32_t prev_start_time = 0;
uint32_t execution_period = 0xFFFFFFFF;
const static uint32_t negative_time_delay = 700;
uint8_t received_trigger = 0;
uint8_t lock_transmission = 0;

void adc_init(void) {
	// Setup output pin which starts ADC conversions
	setup_pin_CONVST();

	// Setup input pin which triggers ADC sampling (from AMDC)
	setup_pin_SYNC_ADC();
}

#if defined(TARGET_AMDS)

void adc_sample_and_transmit_fast_path(uint16_t *sample_data_out) {
	// alert daisy chained AMDSs to begin converting
	GPIO_TOGGLE_PIN(GPIOD, GPIO_PIN_1);
	// 1. Start all ADC conversions.
	SET_PIN_CONVST12_HIGH;
	SET_PIN_CONVST34_HIGH;
	SET_PIN_CONVST56_HIGH;
	SET_PIN_CONVST78_HIGH;

	// Timing optimization: do work before the wait state below.

	uint32_t start_cycles = DWT->CYCCNT;
	// reset DMA routing state machine
	try_reset_routing_state();

	// Calculate 1 microseconds in CPU cycles (integer math safe)
	uint32_t wait_cycles = (SystemCoreClock / 1000000);

	// Deterministic wait for exactly 1300ns using hardware cycles, not NOPs
	while ((DWT->CYCCNT - start_cycles) < wait_cycles) {
		// Spin perfectly safely
	}

	// 2. Start the SCLKs
	drv_spi_start_read_two_16bits(SPI1);
	drv_spi_start_read_two_16bits(SPI4);
	drv_spi_start_read_two_16bits(SPI5);
	drv_spi_start_read_two_16bits(SPI6);

	// 3. Wait and read first ADC data (Channels 0, 1, 2, 3)
	drv_spi_finish_read_one_16bits(SPI1, &sample_data_out[3]);
	drv_spi_finish_read_one_16bits(SPI4, &sample_data_out[1]);
	drv_spi_finish_read_one_16bits(SPI5, &sample_data_out[0]);
	drv_spi_finish_read_one_16bits(SPI6, &sample_data_out[2]);

	// Timing optimization: wait for only the last SPI that we started
	drv_spi_wait_for_RX(SPI6);

	// Read second ADC data (Channels 4, 5, 6, 7)
	drv_spi_get_DR(SPI1, &sample_data_out[7]);
	drv_spi_get_DR(SPI4, &sample_data_out[5]);
	drv_spi_get_DR(SPI5, &sample_data_out[4]);
	drv_spi_get_DR(SPI6, &sample_data_out[6]);

	// End conversion
	SET_PIN_CONVST12_LOW;
	SET_PIN_CONVST34_LOW;
	SET_PIN_CONVST56_LOW;
	SET_PIN_CONVST78_LOW;

	uint8_t wr_1 = tracker1.read_index;
	uint8_t wr_2 = tracker2.read_index;

	for (int8_t i = 0; i < 4; i++) {
		if (SENSOR_MASK_LUT[i] & active_sensor_mask) {
			DAISY_RX1_Pool[--wr_1] = (uint8_t) sample_data_out[i];
			DAISY_RX1_Pool[--wr_1] = (uint8_t) (sample_data_out[i] >> 8);
		}
		if (SENSOR_MASK_LUT[i + 4] & active_sensor_mask) {
			DAISY_RX2_Pool[--wr_2] = (uint8_t) sample_data_out[i + 4];
			DAISY_RX2_Pool[--wr_2] = (uint8_t) (sample_data_out[i + 4] >> 8);
		}
	}

//	if (!received_trigger) {
//		lock_transmission = 0;
//		return;
//	}
	__disable_irq();
	tracker1.read_index = wr_1;
	tracker2.read_index = wr_2;
	process_routing();
	__enable_irq();
	NVIC_ClearPendingIRQ(EXTI3_IRQn);
	__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_3);
	NVIC_ClearPendingIRQ(EXTI3_IRQn);
	__disable_irq();
	process_routing();	// Just in case anything got missed the first time.
	__enable_irq();
}

void trigger_sensor_read_and_transmit(void) {
#ifdef BENCHMARK_MODE
    // =========================================================================
    // INJECT MOCK DMA DATA FOR BENCHMARKING
    // Simulates 8 packets (24 bytes) arriving instantly on the SYNC edge.
    // =========================================================================
    try_reset_routing_state();
    uint8_t current_head = mock_dma_write_head;
    for (int i = 0; i < 24; i++) {
        uint8_t idx = (uint8_t) (current_head + i);
        if (i % 3 == 0) {
            DAISY_RX1_Pool[idx] = 0x90; // Valid Header
            DAISY_RX2_Pool[idx] = 0x90;
        } else {
            DAISY_RX1_Pool[idx] = 0xAA; // Dummy Payload Data
            DAISY_RX2_Pool[idx] = 0xBB;
        }
    }
    // Instantly advance the mock hardware write head
    mock_dma_write_head = (uint8_t) (current_head + 24);
#endif

	uint16_t new_data[8] = { 0 };
	adc_sample_and_transmit_fast_path(new_data);
}

// This ISR is triggered by the AMDC to sync the ADC
// conversions to the AMDC PWM carrier waveform. In
// this ISR, all the mainboard ADCs should be sampled.
void EXTI3_IRQHandler(void) {
	execution_period *= 3;
	execution_period += DWT->CYCCNT - prev_start_time;
	execution_period >>= 2;
	prev_start_time = DWT->CYCCNT;
	received_trigger = 1;
	if (lock_transmission) {
		NVIC_ClearPendingIRQ(EXTI3_IRQn);
		__HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_3);
		NVIC_ClearPendingIRQ(EXTI3_IRQn);
		return;
	}
	lock_transmission = 1;
	trigger_sensor_read_and_transmit();
	lock_transmission = 0;
}

void check_timings_read_data(void) {
	if (DWT->CYCCNT - prev_start_time
			> execution_period - negative_time_delay) {
		lock_transmission = 1;
		received_trigger = 0;
		trigger_sensor_read_and_transmit();
		lock_transmission = 0;
	}
}

void try_read_sensors_before_trigger(void) {
	if (lock_transmission)
		return;
	check_timings_read_data();
}

#elif defined(TARGET_2S)

static void adc_sample_1_5_daughtercards(uint16_t *sample_data_out)
{
    // This function has been optimized for very
    // fast operation of SPI5 interface
    // to cards 1 and 5.
    //
    // It directly manipulates the SPI peripherals'
    // registers to read in data from the ADCs. The ordering
    // of operations may look strange, but this is to minimize
    // wait time of the various APB interconnects in the MCU.
    //
    // The ADC devices support a max of 400ksps. Looking at
    // the waveforms from this function, the CONVST line is
    // asserted for effectively 280kHz... It could be faster,
    // but its not terrible...

    // Start all ADC conversions.
    // ADC conversion triggered by CONVST56 connects to SPI5
    SET_PIN_CONVST56_HIGH;

    // Wait for ADC conversion to complete (per datasheet, >= 1300ns
    // Each NOP takes 5ns, unrolled so branches don't affect timing...
    //
    // We need 260 NOPs
    NOP256;
    NOP4;

    // Smartly read all data from ADC.
    // This starts the SPI peripheral,then waits for it to
    // complete and gets the resulting data.

    // Start the SCLKs
    drv_spi_start_read_two_16bits(SPI5);

    // Wait and read first ADC data
    drv_spi_finish_read_one_16bits(SPI5, &sample_data_out[0]);

    // Wait for second ADC data to complete
    drv_spi_wait_for_RX(SPI5);

    // End conversion
    SET_PIN_CONVST56_LOW;

    // Read second ADC data
    drv_spi_get_DR(SPI5, &sample_data_out[4]);
}

void adc_sample_and_transmit_1_5_fast_path(uint16_t *sample_data_out)
{
    // 1. Start all ADC conversions.
    SET_PIN_CONVST56_HIGH;

    // Timing optimization: do work before the wait state below.

    uint32_t start_cycles = DWT->CYCCNT;
    // reset DMA routing state machine
    try_reset_routing_state();

    // Calculate 1.3 microseconds in CPU cycles (integer math safe)
    uint32_t wait_cycles = (SystemCoreClock / 1000000) * 13 / 10;

    // Deterministic wait for exactly 1300ns using hardware cycles, not NOPs
    while ((DWT->CYCCNT - start_cycles) < wait_cycles) {
        // Spin perfectly safely
    }

    // 2. Start the SCLK
    drv_spi_start_read_two_16bits(SPI5);

    // 3. Wait and read first ADC data (Channels 0, 1, 2, 3)
    drv_spi_finish_read_one_16bits(SPI5, &sample_data_out[0]);

    // Timing optimization: wait for only the last SPI that we started
    drv_spi_wait_for_RX(SPI5);

    drv_uart_putc_fast(USART2, 0x90);
    drv_uart_putc_fast(USART3, 0x90);

    // Read second ADC data (Channels 4, 5, 6, 7)
    drv_spi_get_DR(SPI5, &sample_data_out[4]);

    // don't send first header
    drv_uart_putc_fast(USART2, (uint8_t) (sample_data_out[0] >> 8));
    drv_uart_putc_fast(USART3, (uint8_t) (sample_data_out[4] >> 8));

    drv_uart_putc_fast(USART2, (uint8_t) sample_data_out[0]);
    drv_uart_putc_fast(USART3, (uint8_t) sample_data_out[4]);

    // End conversion
    SET_PIN_CONVST56_LOW;
}

// This ISR is for the FBC and is triggered by the
// AMDC to sync the ADCconversions to the AMDC PWM
// carrier waveform. In this ISR, on 2 ADCs should be sampled.
// void EXTI15_10_IRQHandler(void)
void EXTI15_10_IRQHandler(void)
{
    // alert daisy chained AMDSs to begin converting
    GPIO_TOGGLE_PIN(GPIOG, GPIO_PIN_14);

#ifdef BENCHMARK_MODE
    // =========================================================================
    // INJECT MOCK DMA DATA FOR BENCHMARKING
    // Simulates 8 packets (24 bytes) arriving instantly on the SYNC edge.
    // =========================================================================
    try_reset_routing_state();
    uint8_t current_head = mock_dma_write_head;
    for (int i = 0; i < 3; i++) {
        uint8_t idx = (uint8_t) (current_head + i);
        if (i == 0) {
            DAISY_RX1_Pool[idx] = 0x90; // Valid Header
            DAISY_RX2_Pool[idx] = 0x90;
        } else if (i == 3) {
            DAISY_RX1_Pool[idx] = 0x94; // Valid Header
            DAISY_RX2_Pool[idx] = 0x94;
        } else {
            DAISY_RX1_Pool[idx] = 0xAA; // Dummy Payload Data
            DAISY_RX2_Pool[idx] = 0xBB;
        }
    }
    // Instantly advance the mock hardware write head
    mock_dma_write_head = (uint8_t) (current_head + 3);
#endif

    uint16_t new_data[8] = { 0 };

    // =========================================================================
    // FAST PATH: Integrated Sampling and Transmission!
    // =========================================================================
    if (active_sensor_mask == 0x11) {
        adc_sample_and_transmit_1_5_fast_path(new_data);
    }
    // =========================================================================
    // SLOW PATH: Safe loop for Partial Masks
    // =========================================================================
    else {
#ifndef BENCHMARK_MODE
        try_reset_routing_state();
#endif
        adc_sample_1_5_daughtercards(new_data);

        bool u3 = false;
        bool u2 = false;
        uint8_t header = 0x90;

        if (active_sensor_mask & (1 << 0)) {
            drv_uart_putc_fast(USART2, header);
            u2 = true;
            drv_uart_putc_fast(USART2, (uint8_t) (new_data[0] >> 8));
        }
        if (active_sensor_mask & (1 << 4)) {
            drv_uart_putc_fast(USART3, header);
            u3 = true;
            drv_uart_putc_fast(USART3, (uint8_t) (new_data[4] >> 8));
        }
        if (u2)
            drv_uart_putc_fast(USART2, (uint8_t) (new_data[0]));
        if (u3)
            drv_uart_putc_fast(USART3, (uint8_t) (new_data[4]));
    }

    uint32_t start_cycles = DWT->CYCCNT;

    // Calculate 1 microseconds in CPU cycles (integer math safe)
    uint32_t wait_cycles = (SystemCoreClock / 1000000);

    while (!(USART2->ISR & UART_FLAG_TC) && !(USART3->ISR & UART_FLAG_TC)
           && ((DWT->CYCCNT - start_cycles) < wait_cycles)) {
    }

    // Handle any DMA data that has been received from daisy chain
    try_process_routing(); // This try function is thread safe

    // Clear all pending IRQs for ADC conversions at the
    // end of this ISR so that the system realigns the
    // ADC conversions with the SYNC signal from the AMDC.
    //
    // For some reason, this only works if we call both of these:
    NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_11);
    NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
}
#else
#error "Please define a target board (TARGET_AMDS or TARGET_2S)!"
#endif

static void setup_pin_CONVST(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOF_CLK_ENABLE();
	__HAL_RCC_GPIOG_CLK_ENABLE();

// Configure GPIO pin Output Level
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_10 | GPIO_PIN_11, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOF, GPIO_PIN_6, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOG, GPIO_PIN_8, GPIO_PIN_RESET);

// Configure GPIO pins
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

	GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11;
	HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = GPIO_PIN_6;
	HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = GPIO_PIN_8;
	HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
}

static void setup_pin_SYNC_ADC(void) {
// ADC Sync is a square wave input where every edge should
// trigger a sampling event from the mainboard ADCs.
//
// These edges are aligned to the PWM carrier on the AMDC.

	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

#if defined(TARGET_AMDS)
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();

// Configure GPIO pin Output Level
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_1, GPIO_PIN_SET);

// Configure GPIO pins
	GPIO_InitStruct.Pin = GPIO_PIN_3;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = GPIO_PIN_1;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

// EXTI interrupt init
	HAL_NVIC_SetPriority(EXTI3_IRQn, 10, 0);
	HAL_NVIC_EnableIRQ(EXTI3_IRQn);
#elif defined(TARGET_2S)
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    // Configure GPIO pin Output Level
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_14, GPIO_PIN_SET);

    // Configure GPIO pins
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_14;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    // EXTI interrupt init
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 10, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
#else
#error "Please define a target board (TARGET_AMDS or TARGET_2S)!"
#endif
}
