#include "tx.h"
#include "adc.h"
#include "drv_uart.h"
#include "platform.h"

// Called after ADC conversions have been completed,
// to send sampled data back to AMDC
//
void transmit_samples(void)
{
    // Send header before we compute anything
    // to get the UART warmed up and running!
    uint8_t first_header = 0x90;
    drv_uart_putc_fast(USART2, first_header);
    drv_uart_putc_fast(USART3, first_header);

    // Get latest data from ADC driver (non-blocking)
    uint16_t bits[8];
    adc_latest_bits(bits);

    // Send data out over UART
    for (int i = 0; i < 4; i++) {
        uint16_t sample1 = bits[(4 * 0) + i];
        uint16_t sample2 = bits[(4 * 1) + i];

        // Send header (not the first one)
        if (i > 0) {
            // Send header as:
            // bits[7:2] = 100100
            // bits[1:0] = # of DC (2 bits, 0..3)
            uint8_t header = 0x90;
            header |= (0x03 & i);

            drv_uart_putc_fast(USART2, header);
            drv_uart_putc_fast(USART3, header);
        }

        // Send ADC sample data MSBs
        drv_uart_putc_fast(USART2, (uint8_t)(sample1 >> 8));
        drv_uart_putc_fast(USART3, (uint8_t)(sample2 >> 8));

        // Send ADC sample data LSBs
        drv_uart_putc_fast(USART2, (uint8_t)(sample1 & 0x00FF));
        drv_uart_putc_fast(USART3, (uint8_t)(sample2 & 0x00FF));
    }

    // Wait for entire UART transmission to complete
    drv_uart_wait_TC(USART2);
    drv_uart_wait_TC(USART3);
}


