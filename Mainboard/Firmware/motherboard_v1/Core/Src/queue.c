#include "queue.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

uint32_t current_id;
amds_queue_t sensor_queue;
amds_item_t sensor_buffer[AMDS_QUEUE_SIZE];

void queue_init(amds_queue_t *q, amds_item_t *buffer, uint32_t capacity)
{
    // capacity must be non-zero and power of two
    q->head = 0;
    q->tail = 0;
    q->buffer = buffer;
    q->capacity = capacity;
    q->mask = capacity - 1;
}

void queue_reset(amds_queue_t *q)
{
    q->head = 0;
    q->tail = 0;
}

bool queue_enqueue_from_isr(amds_queue_t *q, const amds_item_t *item, uint32_t item_id)
{
    uint32_t head = q->head;
    uint32_t tail = q->tail;
    uint32_t next = head + 1;

    // full if number of items would exceed capacity
    if ((next - tail) > q->capacity) {
        return false;
    }

    uint32_t idx = head & q->mask;

    // copy and attach item_id (producer-only)
    amds_item_t tmp = *item;
    tmp.item_id = item_id;
    q->buffer[idx] = tmp;

    q->head = next;
    return true;
}

bool queue_dequeue(amds_queue_t *q, amds_item_t *out)
{
    uint32_t tail = q->tail;
    uint32_t head = q->head;

    if (tail == head) {
        return false;
    }

    uint32_t idx = tail & q->mask;
    *out = q->buffer[idx];

    q->tail = tail + 1;
    return true;
}

uint32_t queue_count(const amds_queue_t *q)
{
    return (uint32_t)(q->head - q->tail);
}
