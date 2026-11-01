#ifndef TRANS_QUEUE_H
#define TRANS_QUEUE_H

#include <stdint.h>
#include "data_layout.h"

#define TQ_CAPACITY   100u
#define TQ_MSG_MAX    DL_FRAME_BYTES   /* 9-byte binary frame */

#define TQ_OK              0
#define TQ_DROPPED_OLDEST  1
#define TQ_ERR_ARG        (-1)
#define TQ_ERR_TOO_BIG    (-2)
#define TQ_EMPTY          (-3)
#define TQ_ERR_STALE      (-4)

typedef struct {
    uint32_t seq;
    uint16_t len;
    uint8_t  data[TQ_MSG_MAX];
} tq_msg_t;

void     tq_init(void);
int      tq_push(const uint8_t *data, uint16_t len);
int      tq_peek_oldest(tq_msg_t *out);
int      tq_ack_oldest(uint32_t seq);
uint16_t tq_count(void);
uint32_t tq_discarded(void);

#endif