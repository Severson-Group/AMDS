#ifndef QUEUE_H
#define QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

extern uint32_t current_id;

typedef struct {
    uint16_t samples[8];
    uint32_t item_id;
} amds_item_t;

typedef struct {
    volatile uint32_t head; // written to
    volatile uint32_t tail; // read from
    uint32_t mask;          // for fast indexing
    uint32_t capacity;
    amds_item_t *buffer;
} amds_queue_t;

// must be power of two
#define AMDS_QUEUE_SIZE 8

extern amds_queue_t sensor_queue;
extern amds_item_t sensor_buffer[AMDS_QUEUE_SIZE];

// Initialize queue: buffer must point to an array of amds_item_t of size capacity
// capacity MUST be a power of two.
void queue_init(amds_queue_t *q, amds_item_t *buffer, uint32_t capacity);
void queue_reset(amds_queue_t *q);

// enqueue: copies item into buffer and advances head.
// Returns true on success, false if queue full (item not enqueued).
bool queue_enqueue_from_isr(amds_queue_t *q, const amds_item_t *item, uint32_t item_id);

// dequeue: returns true and fills out when an item available.
bool queue_dequeue(amds_queue_t *q, amds_item_t *out);

// Number of items currently in queue (head - tail)
uint32_t queue_count(const amds_queue_t *q);

#endif // QUEUE_H
