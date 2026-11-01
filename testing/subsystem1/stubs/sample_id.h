/* PC tests only. The binary frame has no timestamp, so a test numbers its samples by
 * hiding the number in three fields: dist (63 values) x speed (63) x dir (360) = 1.4 M. */
#ifndef SAMPLE_ID_H
#define SAMPLE_ID_H
#include "telemetry.h"
static inline void sid_set(telemetry_sample_t *s, uint32_t id)
{
    s->dist  = (uint8_t)(id % 63u);
    s->speed = (uint8_t)((id / 63u) % 63u);
    s->dir   = (uint16_t)(id / 3969u);          /* ids up to 1,428,839 */
}
/* returns 1 and the number if the bytes are a valid 9-byte frame, else 0 */
static inline int sid_get(const void *frame, int len, uint32_t *id)
{
    telemetry_sample_t d;
    if (telemetry_unpack((const uint8_t *)frame, (uint32_t)len, &d) != TELEM_OK) { return 0; }
    *id = ((uint32_t)d.dir * 63u + d.speed) * 63u + d.dist;
    return 1;
}
#endif