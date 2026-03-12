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

    // Send board's own ADC data out over UART
    for (int i = 0; i < 4; i++) {
        uint16_t sample1 = bits[(4 * 0) + i];
        uint16_t sample2 = bits[(4 * 1) + i];

        // Send header (not the first one)
        if (i > 0) {
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

    // Now attempt to send AMDS samples (if ready) using same header scheme.
    // adc_latest_amds() will populate the array only when the corresponding
    // amds_samples_ready pairs are set.
    uint16_t amds[16] = {0};
    adc_latest_amds(amds);
    
    first_header = 0x94;

    // First AMDS set: indices 0..7 (requires amds_samples_ready[0] && [1])
    if (amds_samples_ready[0] && amds_samples_ready[1]) {
        // Send header for this set
        drv_uart_putc_fast(USART2, first_header);
        drv_uart_putc_fast(USART3, first_header);

        for (int i = 0; i < 4; i++) {
            if (i > 0) {
                uint8_t header = 0x94;
                header |= (0x07 & i);
                drv_uart_putc_fast(USART2, header);
                drv_uart_putc_fast(USART3, header);
            }

            uint16_t sample1 = amds[(4 * 0) + i]; // amds[0..3]
            uint16_t sample2 = amds[(4 * 1) + i]; // amds[4..7]

            // Send ADC sample data MSBs
            drv_uart_putc_fast(USART2, (uint8_t)(sample1 >> 8));
            drv_uart_putc_fast(USART3, (uint8_t)(sample2 >> 8));

            // Send ADC sample data LSBs
            drv_uart_putc_fast(USART2, (uint8_t)(sample1 & 0x00FF));
            drv_uart_putc_fast(USART3, (uint8_t)(sample2 & 0x00FF));
        }

        // Clear flags for this set
        amds_samples_ready[0] = false;
        amds_samples_ready[1] = false;

        first_header = 0x98;

		// Second AMDS set: indices 8..15 (requires amds_samples_ready[2] && [3])
        // Only if the first set was successful
		if (amds_samples_ready[2] && amds_samples_ready[3]) {
			drv_uart_putc_fast(USART2, first_header);
			drv_uart_putc_fast(USART3, first_header);

			for (int i = 0; i < 4; i++) {
				if (i > 0) {
					uint8_t header = 0x98;
					header |= (0x0B & i);
					drv_uart_putc_fast(USART2, header);
					drv_uart_putc_fast(USART3, header);
				}

				uint16_t sample1 = amds[8 + (4 * 0) + i];  // amds[8..11]
				uint16_t sample2 = amds[8 + (4 * 1) + i];  // amds[12..15]

				// Send ADC sample data MSBs
				drv_uart_putc_fast(USART2, (uint8_t)(sample1 >> 8));
				drv_uart_putc_fast(USART3, (uint8_t)(sample2 >> 8));

				// Send ADC sample data LSBs
				drv_uart_putc_fast(USART2, (uint8_t)(sample1 & 0x00FF));
				drv_uart_putc_fast(USART3, (uint8_t)(sample2 & 0x00FF));
			}

			// Clear flags for this set
			amds_samples_ready[2] = false;
			amds_samples_ready[3] = false;
		}
    }

    // Wait for entire UART transmission to complete
    drv_uart_wait_TC(USART2);
    drv_uart_wait_TC(USART3);

    uart4_amds_sample_count = 0;
	uart5_amds_sample_count = 0;
}


