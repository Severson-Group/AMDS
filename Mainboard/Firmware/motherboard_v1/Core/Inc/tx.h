#ifndef TX_H
#define TX_H

#include "platform.h"
#include <stdint.h>
#include <stdbool.h>

#define NUM_SETS 3       // 0 = local data, 1 = AMDS 1, 2 = AMDS 2
#define PACKETS_PER_SET 8

typedef struct {
    uint8_t header;
    uint8_t msb;
    uint8_t lsb;
} packet_t;

// Global state arrays
extern volatile packet_t tx_packets[NUM_SETS * PACKETS_PER_SET];
extern volatile bool packet_ready[NUM_SETS * PACKETS_PER_SET];
extern bool packet_sent[NUM_SETS * PACKETS_PER_SET];

// Flag set by EXTI Sync Interrupt
extern volatile bool sync_event_flag;

void process_transmissions(void);

void transmit_samples(void);

#endif // TX_H
