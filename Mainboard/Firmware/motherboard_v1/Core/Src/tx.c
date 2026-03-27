#include "tx.h"

volatile packet_t tx_packets[NUM_SETS * PACKETS_PER_SET];
volatile bool packet_ready[NUM_SETS * PACKETS_PER_SET];
bool packet_sent[NUM_SETS * PACKETS_PER_SET];

volatile bool sync_event_flag = false; // Set this to true in your EXTI ISR

// Scans the state arrays, builds contiguous buffers, and fires DMA
void process_transmissions(void) {
    // We only need a tiny 3-byte static buffer for each UART now.
    // (Static is still required so the memory persists while DMA is working in the background)
    static uint8_t dma_tx2[3];
    static uint8_t dma_tx3[3];

    // Scan the array for ready/unsent packets
    for (int pkt = 0; pkt < NUM_SETS * PACKETS_PER_SET; pkt++) {
    	// Check hardware states once at the start
		while (huart2.gState != HAL_UART_STATE_READY) {
			asm("nop");
		}

		while (huart3.gState != HAL_UART_STATE_READY) {
			asm("nop");
		}

        if (packet_ready[pkt] && !packet_sent[pkt]) {

            // Route to UART2
            if ((pkt >= 0 && pkt <= 3) || (pkt >= 8 && pkt <= 11) || (pkt >= 16 && pkt <= 19)) {

                // Copy exactly 3 bytes into the UART2 DMA buffer
                dma_tx2[0] = tx_packets[pkt].header;
                dma_tx2[1] = tx_packets[pkt].msb;
                dma_tx2[2] = tx_packets[pkt].lsb;

                packet_sent[pkt] = true;

                HAL_UART_Transmit_DMA(&huart2, dma_tx2, 3);
            }

            // Route to UART3
            if ((pkt >= 4 && pkt <= 7) || (pkt >= 12 && pkt <= 15) || (pkt >= 20 && pkt <= 23)) {

                // Copy exactly 3 bytes into the UART3 DMA buffer
                dma_tx3[0] = tx_packets[pkt].header;
                dma_tx3[1] = tx_packets[pkt].msb;
                dma_tx3[2] = tx_packets[pkt].lsb;

                packet_sent[pkt] = true;

                HAL_UART_Transmit_DMA(&huart3, dma_tx3, 3);
            }
        }
    }
}
























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

// Called after ADC conversions have been completed,
// to send sampled data back to AMDC
//
void transmit_samples(void)
{
    // 2. Create a local buffer for this transmission "burst"
    static uint8_t tx_data2[36];
	static uint8_t tx_data3[36];
    uint8_t idx = 0;

    uint16_t bits[8];
    adc_latest_bits(bits);

    for (int i = 0; i < 4; i++) {
    	uint8_t header = (i == 0) ? 0x90 : (0x90 | (0x03 & i));

    	tx_data2[idx] = header;
    	tx_data3[idx] = header;
		idx++;

        tx_data2[idx] = (uint8_t)(bits[i] >> 8);   // MSB
        tx_data3[idx] = (uint8_t)(bits[i + 4] >> 8);   // MSB
        idx++;

        tx_data2[idx] = (uint8_t)(bits[i] & 0xFF); // LSB
        tx_data3[idx] = (uint8_t)(bits[i + 4] & 0xFF); // LSB
        idx++;
    }

    // 1. Process incoming UARTs first
    NOP256;
    NOP256;
    NOP256;
	NOP256;
	NOP256;
	NOP256;
	NOP256;
	NOP256;
	NOP256;
	NOP256;
	NOP256;
	NOP256;
	NOP256;
	NOP256;
	NOP256;
	NOP256;
	NOP256;
	NOP256;
    process_uart_fifo(UART4_DMA_Pool, &tracker4, 4);
    process_uart_fifo(UART5_DMA_Pool, &tracker5, 5);

    uint16_t amds[16] = {1};
    adc_latest_amds(amds);

    // ... Add AMDS data to tx_data if ready ...
    if (amds_samples_ready[0]) {
    	for (int i = 0; i < 4; i++) {
			uint8_t header = (i == 0) ? 0x94 : (0x94 | (0x07 & i));

			tx_data2[idx] = header;
			tx_data3[idx] = header;
			idx++;

			tx_data2[idx] = (uint8_t)(amds[i] >> 8);   // MSB
			tx_data3[idx] = (uint8_t)(amds[i + 4] >> 8);   // MSB
			idx++;

			tx_data2[idx] = (uint8_t)(amds[i] & 0xFF); // LSB
			tx_data3[idx] = (uint8_t)(amds[i + 4] & 0xFF); // LSB
			idx++;
		}

    	// Clear flags for this set
		amds_samples_ready[0] = false;

		if (amds_samples_ready[1]) {
			for (int i = 0; i < 4; i++) {
				uint8_t header = (i == 0) ? 0x98 : (0x98 | (0x0B & i));

				tx_data2[idx] = header;
				tx_data3[idx] = header;
				idx++;

				tx_data2[idx] = (uint8_t)(amds[i] >> 8);   // MSB
				tx_data3[idx] = (uint8_t)(amds[i + 4] >> 8);   // MSB
				idx++;

				tx_data2[idx] = (uint8_t)(amds[i] & 0xFF); // LSB
				tx_data3[idx] = (uint8_t)(amds[i + 4] & 0xFF); // LSB
				idx++;
			}

			// Clear flags for this set
			amds_samples_ready[1] = false;
		}
    }

    // 4. Trigger the DMA to send exactly 'idx' bytes
    // This function returns immediately while the hardware sends the data
    if (huart2.gState == HAL_UART_STATE_READY && huart3.gState == HAL_UART_STATE_READY) {
		HAL_UART_Transmit_DMA(&huart2, tx_data2, idx);
		HAL_UART_Transmit_DMA(&huart3, tx_data3, idx);
	}
}






//void transmit_samples(void)
//{
//	// Send header before we compute anything
//    // to get the UART warmed up and running!
//    uint8_t first_header = 0x90;
//    drv_uart_putc_dma(USART2, first_header);
//    drv_uart_putc_dma(USART3, first_header);
//
//    // Get latest data from ADC driver (non-blocking)
//    uint16_t bits[8];
//    adc_latest_bits(bits);
//
//    // Send board's own ADC data out over UART
//    for (int i = 0; i < 4; i++) {
//        uint16_t sample1 = bits[(4 * 0) + i];
//        uint16_t sample2 = bits[(4 * 1) + i];
//
//        // Send header (not the first one)
//        if (i > 0) {
//            uint8_t header = 0x90;
//            header |= (0x03 & i);
//
//            drv_uart_putc_dma(USART2, header);
//            drv_uart_putc_dma(USART3, header);
//        }
//
//        // Send ADC sample data MSBs
//        drv_uart_putc_dma(USART2, (uint8_t)(sample1 >> 8));
//        drv_uart_putc_dma(USART3, (uint8_t)(sample2 >> 8));
//
//        // Send ADC sample data LSBs
//        drv_uart_putc_dma(USART2, (uint8_t)(sample1 & 0x00FF));
//        drv_uart_putc_dma(USART3, (uint8_t)(sample2 & 0x00FF));
//    }
//
//	process_uart_fifo(UART4_DMA_Pool, &tracker4, 4);
//	process_uart_fifo(UART5_DMA_Pool, &tracker5, 5);
//    // Now attempt to send AMDS samples (if ready) using same header scheme.
//    // adc_latest_amds() will populate the array only when the corresponding
//    // amds_samples_ready pairs are set.
//    uint16_t amds[16] = {0};
//    adc_latest_amds(amds);
//
//    first_header = 0x94;
//
//    // First AMDS set: indices 0..7 (requires amds_samples_ready[0] && [1])
//    if (amds_samples_ready[0] && amds_samples_ready[1]) {
//        // Send header for this set
//        drv_uart_putc_dma(USART2, first_header);
//        drv_uart_putc_dma(USART3, first_header);
//
//        for (int i = 0; i < 4; i++) {
//            if (i > 0) {
//                uint8_t header = 0x94;
//                header |= (0x07 & i);
//                drv_uart_putc_dma(USART2, header);
//                drv_uart_putc_dma(USART3, header);
//            }
//
//            uint16_t sample1 = amds[(4 * 0) + i]; // amds[0..3]
//            uint16_t sample2 = amds[(4 * 1) + i]; // amds[4..7]
//
//            // Send ADC sample data MSBs
//            drv_uart_putc_dma(USART2, (uint8_t)(sample1 >> 8));
//            drv_uart_putc_dma(USART3, (uint8_t)(sample2 >> 8));
//
//            // Send ADC sample data LSBs
//            drv_uart_putc_dma(USART2, (uint8_t)(sample1 & 0x00FF));
//            drv_uart_putc_dma(USART3, (uint8_t)(sample2 & 0x00FF));
//        }
//
//        // Clear flags for this set
//        amds_samples_ready[0] = false;
//        amds_samples_ready[1] = false;
//
//        first_header = 0x98;
//
//		// Second AMDS set: indices 8..15 (requires amds_samples_ready[2] && [3])
//        // Only if the first set was successful
//		if (amds_samples_ready[2] && amds_samples_ready[3]) {
//			drv_uart_putc_dma(USART2, first_header);
//			drv_uart_putc_dma(USART3, first_header);
//
//			for (int i = 0; i < 4; i++) {
//				if (i > 0) {
//					uint8_t header = 0x98;
//					header |= (0x0B & i);
//					drv_uart_putc_dma(USART2, header);
//					drv_uart_putc_dma(USART3, header);
//				}
//
//				uint16_t sample1 = amds[8 + (4 * 0) + i];  // amds[8..11]
//				uint16_t sample2 = amds[8 + (4 * 1) + i];  // amds[12..15]
//
//				// Send ADC sample data MSBs
//				drv_uart_putc_dma(USART2, (uint8_t)(sample1 >> 8));
//				drv_uart_putc_dma(USART3, (uint8_t)(sample2 >> 8));
//
//				// Send ADC sample data LSBs
//				drv_uart_putc_dma(USART2, (uint8_t)(sample1 & 0x00FF));
//				drv_uart_putc_dma(USART3, (uint8_t)(sample2 & 0x00FF));
//			}
//
//			// Clear flags for this set
//			amds_samples_ready[2] = false;
//			amds_samples_ready[3] = false;
//		}
//    }
//
//    // Wait for entire UART transmission to complete
//    drv_uart_wait_TC(USART2);
//    drv_uart_wait_TC(USART3);
//
//    uart4_amds_sample_count = 0;
//	uart5_amds_sample_count = 0;
//}


