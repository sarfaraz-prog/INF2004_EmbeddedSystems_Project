/*
 * telemetry.h -- Buddy 1. Packs one sample into the 9-byte binary frame
 * (layout in data_layout.h) and reads it back. Plain C, no kernel calls.
 * Tasks keep natural-width values; packing happens only here, at send time.
 */
#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>
#include "data_layout.h"

#define TELEM_OK         0
#define TELEM_ERR_ARG   (-1)   /* NULL pointer or buffer too small */
#define TELEM_ERR_RANGE (-2)   /* a value does not fit its field */

typedef struct {
    uint8_t  state;        /* 0-7 (dl_state_t)            */
    uint8_t  error;        /* 0/1                         */
    uint8_t  wifi_loss;    /* 0/1 status                  */
    uint8_t  low_batt;     /* 0/1 status                  */
    uint8_t  drop;         /* 0/1 messages were dropped   */
    uint8_t  hump;         /* 0/1                         */
    uint8_t  speed;        /* cm/s 0-62, 63 = invalid     */
    uint8_t  bc_dir;       /* 0-2 (3 spare)               */
    uint8_t  bc_err;       /* 0/1                         */
    uint8_t  line;         /* 0-3                         */
    uint8_t  dist;         /* cm 0-62, 63 = no echo       */
    uint16_t dir;          /* deg 0-359, 511 = invalid    */
    uint8_t  hump_h;       /* mm 0-128, 255 = invalid     */
    int16_t  tilt;         /* deg -180..180, TILT_BAD     */
    int16_t  roll;         /* deg -180..180, TILT_BAD     */
    uint8_t  seq;          /* 0-15, set by comms          */
} telemetry_sample_t;

#define TELEM_ANGLE_INVALID  INT16_MIN   /* "no tilt/roll reading" */

/* Write DL_FRAME_BYTES bytes into out. Returns TELEM_OK or TELEM_ERR_*. */
int telemetry_pack(const telemetry_sample_t *s, uint8_t *out, uint32_t size);

/* Read a frame back (PC tools and tests). Returns TELEM_OK or TELEM_ERR_*;
 * a wrong version byte or length gives TELEM_ERR_ARG. */
int telemetry_unpack(const uint8_t *in, uint32_t len, telemetry_sample_t *s);

/* One command byte -> DL_CMD_START/STOP/RESET, or 0 if not valid. */
uint8_t telemetry_cmd_decode(const uint8_t *in, uint32_t len);

#endif