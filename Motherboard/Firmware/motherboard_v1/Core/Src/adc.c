#include "adc.h"
#include "drv_spi.h"
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

// As the code runs, these state variables are updated in ISRs
// Ping-pong buffer of latest samples
static volatile bool is_ignore_next_sample = false;
static volatile uint8_t read_buffer_number = 0;
static volatile uint8_t write_buffer_number = 1;
static volatile uint16_t latest_valid_adc_data0[8] = { 0 };
static volatile uint16_t latest_valid_adc_data1[8] = { 0 };

void adc_init(void)
{
    // Setup output pin which starts ADC conversions
    setup_pin_CONVST();

    // Setup input pin which triggers ADC sampling (from AMDC)
    setup_pin_SYNC_ADC();
}

void adc_ignore_next_sample(void)
{
    is_ignore_next_sample = true;
}

// NOTE: this function is called from the TX ISR,
// which can preempt ANY CODE which is running!!
//
// Most importantly, this preempts our ADC conversion ISR!
//
// To prove correctness, we just need to verify the invariant
// that the `read_buffer_number` variable and corresponding buffer
// always contain valid data.
void adc_latest_bits(uint16_t *output)
{
    volatile uint16_t *data = NULL;
    if (read_buffer_number == 0) {
        data = latest_valid_adc_data0;
    } else {
        data = latest_valid_adc_data1;
    }

    // Give user their data (unrolled for speed)
    output[0] = data[0];
    output[1] = data[1];
    output[2] = data[2];
    output[3] = data[3];
    output[4] = data[4];
    output[5] = data[5];
    output[6] = data[6];
    output[7] = data[7];
}

static void adc_sample_all_daughtercards(uint16_t *sample_data_out)
{
    // This function has been optimized for very
    // fast operation of all four SPI interfaces
    // to the daughter cards.
    //
    // It directly manipulates the four SPI peripherals'
    // registers to read in data from the ADCs. The ordering
    // of operations may look strange, but this is to minimize
    // wait time of the various APB interconnects in the MCU.
    //
    // The ADC devices support a max of 400ksps. Looking at
    // the waveforms from this function, the CONVST line is
    // asserted for effectively 280kHz... It could be faster,
    // but its not terrible...

    // Start all ADC conversions. We start them in order since
    // the ADC connected to CONVST12 will probably get done first.
    //
    // ADC conversion triggered by CONVST12 connects to SPI1
    // ADC conversion triggered by CONVST34 connects to SPI4
    // ADC conversion triggered by CONVST56 connects to SPI5
    // ADC conversion triggered by CONVST78 connects to SPI6
    SET_PIN_CONVST12_HIGH;
    SET_PIN_CONVST34_HIGH;
    SET_PIN_CONVST56_HIGH;
    SET_PIN_CONVST78_HIGH;

    // Wait for ADC conversion to complete (per datasheet, >= 1300ns
    // Each NOP takes 5ns, unrolled so branches don't affect timing...
    //
    // We need 260 NOPs
    NOP256;
    NOP4;

    // Smartly read all data from all ADCs at the same time.
    // This starts all the SPI peripherals effectively in parallel,
    // then waits for them to complete and gets the resulting data.

    // Start the SCLKs
    drv_spi_start_read_two_16bits(SPI1);
    drv_spi_start_read_two_16bits(SPI4);
    drv_spi_start_read_two_16bits(SPI5);
    drv_spi_start_read_two_16bits(SPI6);

    // Wait and read first ADC data
    //
    // The strange ordering of sample data indexing is
    // to correct for PCB layout pin swapping issues.
    drv_spi_finish_read_one_16bits(SPI1, &sample_data_out[3]);
    drv_spi_finish_read_one_16bits(SPI4, &sample_data_out[1]);
    drv_spi_finish_read_one_16bits(SPI5, &sample_data_out[0]);
    drv_spi_finish_read_one_16bits(SPI6, &sample_data_out[2]);

    // Wait for second ADC data to complete
    drv_spi_wait_for_RX(SPI1);
    drv_spi_wait_for_RX(SPI4);
    drv_spi_wait_for_RX(SPI5);
    drv_spi_wait_for_RX(SPI6);

    // End conversion
    SET_PIN_CONVST12_LOW;
    SET_PIN_CONVST34_LOW;
    SET_PIN_CONVST56_LOW;
    SET_PIN_CONVST78_LOW;

    // Read second ADC data
    drv_spi_get_DR(SPI1, &sample_data_out[7]);
    drv_spi_get_DR(SPI4, &sample_data_out[5]);
    drv_spi_get_DR(SPI5, &sample_data_out[4]);
    drv_spi_get_DR(SPI6, &sample_data_out[6]);
}

// This ISR is triggered by the AMDC to sync the ADC
// conversions to the AMDC PWM carrier waveform. In
// this ISR, all the motherboard ADCs should be sampled.
void EXTI3_IRQHandler(void)
{
    // Clear interrupt
    //
    // NOTE: while this ISR is running and sampling ADCs, a lot
    // could happen -- the TX ISR could run, as well as
    // another edge which would trigger this ISR again!
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_3)) {
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_3);
    }

    // Perform the actual SPI transactions
    uint16_t new_data[8] = { 0 };
    adc_sample_all_daughtercards(new_data);

    // Copy data into write buffer destination
    volatile uint16_t *dest = NULL;
    if (write_buffer_number == 0) {
        dest = latest_valid_adc_data0;
    } else {
        dest = latest_valid_adc_data1;
    }

    // Unrolled loop for speed
    dest[0] = new_data[0];
    dest[1] = new_data[1];
    dest[2] = new_data[2];
    dest[3] = new_data[3];
    dest[4] = new_data[4];
    dest[5] = new_data[5];
    dest[6] = new_data[6];
    dest[7] = new_data[7];

    // Only switch read / write pointers if we weren't told to ignore this sample
    if (is_ignore_next_sample) {
        is_ignore_next_sample = false;
    } else {
        // Switch read buffer to where we just put the new data,
        // therefore, satisfying the property that the read_buffer_number
        // always points to a valid set of samples!
        read_buffer_number = 1 - read_buffer_number;

        // Now that read buffer is set, we are safe to update write buffer
        // for the next time this ISR runs...
        write_buffer_number = 1 - write_buffer_number;
    }

    // Clear all pending IRQs for ADC conversions at the
    // end of this ISR so that the system realigns the
    // ADC conversions with the SYNC signal from the AMDC.
    //
    // For some reason, this only works if we call both of these:
    NVIC_ClearPendingIRQ(EXTI3_IRQn);
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_3);
    NVIC_ClearPendingIRQ(EXTI3_IRQn);
}

static void setup_pin_CONVST(void)
{
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
    HAL_NVIC_SetPriority(EXTI3_IRQn, 10, 0);
    HAL_NVIC_EnableIRQ(EXTI3_IRQn);
}
