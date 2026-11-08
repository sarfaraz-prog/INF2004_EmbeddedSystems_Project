/*
 * trans_queue.c 
 * Offline telemetry queue (ring buffer). See trans_queue.h.
 * Statically allocated: 100 x 16 bytes (NFR-RES-03).
 * Not thread-safe by itself: the caller holds a mutex (NFR-MNT-02).
 */
#include "trans_queue.h"

#include <string.h>

static tq_msg_t  s_buf[TQ_CAPACITY];
static uint16_t  s_head;        /* index of the oldest message */
static uint16_t  s_count;       /* messages currently stored */
static uint32_t  s_next_seq;    /* sequence number for the next message */
static uint32_t  s_discarded;   /* messages lost to overflow (FR-COM-11) */

void tq_init(void)
{
    s_head      = 0u;
    s_count     = 0u;
    s_next_seq  = 1u;           /* 0 is never a valid sequence number */
    s_discarded = 0u;
}

int tq_push(const uint8_t *data, uint16_t len)
{
    int      result = TQ_OK;
    uint16_t tail;

    if ((data == NULL) || (len == 0u)) {
        return TQ_ERR_ARG;
    }
    if (len > TQ_MSG_MAX) {
        return TQ_ERR_TOO_BIG;
    }

    if (s_count == TQ_CAPACITY) {
        /* Full: throw away the oldest to make room (FR-COM-10) and count it
         * (FR-COM-11). */
        s_head = (uint16_t)((s_head + 1u) % TQ_CAPACITY);
        s_count--;
        s_discarded++;
        result = TQ_DROPPED_OLDEST;
    }

    tail = (uint16_t)((s_head + s_count) % TQ_CAPACITY);
    s_buf[tail].seq = s_next_seq;
    s_buf[tail].len = len;
    (void)memcpy(s_buf[tail].data, data, len);
    s_next_seq++;
    s_count++;

    return result;
}

int tq_peek_oldest(tq_msg_t *out)
{
    if (out == NULL) {
        return TQ_ERR_ARG;
    }
    if (s_count == 0u) {
        return TQ_EMPTY;
    }
    *out = s_buf[s_head];
    return TQ_OK;
}

int tq_ack_oldest(uint32_t seq)
{
    if (s_count == 0u) {
        return TQ_EMPTY;
    }
    if (s_buf[s_head].seq != seq) {
        return TQ_ERR_STALE;    /* oldest changed since it was peeked */
    }

    s_head = (uint16_t)((s_head + 1u) % TQ_CAPACITY);
    s_count--;

    if (s_count == 0u) {
        s_discarded = 0u;       /* queue fully transmitted (FR-COM-12) */
    }
    return TQ_OK;
}

uint16_t tq_count(void)
{
    return s_count;
}

uint32_t tq_discarded(void)
{
    return s_discarded;
}