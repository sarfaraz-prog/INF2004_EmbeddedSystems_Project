/* telemetry.c -- Buddy 1. Pack/unpack with masks and shifts (no C bit-fields). */
#include "telemetry.h"

/* Put a value into the word / read it back (shift and width come from data_layout.h). */
static void put(uint64_t *w, uint32_t v, uint32_t shift, uint32_t bits)
{
    *w |= ((uint64_t)v & dl_mask(bits)) << shift;
}

static uint32_t get(uint64_t w, uint32_t shift, uint32_t bits)
{
    return (uint32_t)((w >> shift) & dl_mask(bits));
}

static int angle_ok(int16_t a)
{
    return (a == TELEM_ANGLE_INVALID) || ((a >= DL_TILT_MIN_DEG) && (a <= DL_TILT_MAX_DEG));
}

static uint32_t angle_enc(int16_t a)
{
    return (a == TELEM_ANGLE_INVALID) ? DL_TILT_INVALID : (uint32_t)(a + DL_TILT_OFFSET);
}

static int16_t angle_dec(uint32_t v)
{
    return (v == DL_TILT_INVALID) ? TELEM_ANGLE_INVALID : (int16_t)((int32_t)v - DL_TILT_OFFSET);
}

/* Every field must be inside its range (or be its invalid code). */
static int flags_ok(const telemetry_sample_t *s)
{
    return (s->error <= 1u) && (s->wifi_loss <= 1u) && (s->low_batt <= 1u) &&
           (s->drop <= 1u) && (s->hump <= 1u) && (s->bc_err <= 1u);
}

static int fields_ok(const telemetry_sample_t *s)
{
    if (!flags_ok(s) || (s->state > DL_STATE_MAX) || (s->bc_dir > DL_BC_DIR_MAX) || (s->line > DL_LINE_MAX) || (s->seq > DL_SEQ_MAX)) {
        return 0;
    }
    if ((s->speed > DL_SPEED_INVALID) || (s->dist > DL_DIST_INVALID)) { return 0; }
    /* 360..510 would fit in 9 bits but is not a valid direction */
    if ((s->dir > DL_DIR_MAX_DEG) && (s->dir != DL_DIR_INVALID)) { return 0; }
    if ((s->hump_h > DL_HUMP_H_MAX_MM) && (s->hump_h != DL_HUMP_H_INVALID)) { return 0; }
    return angle_ok(s->tilt) && angle_ok(s->roll);
}

int telemetry_pack(const telemetry_sample_t *s, uint8_t *out, uint32_t size)
{
    uint64_t w = 0u;
    uint32_t i;

    if ((!s) || (!out) || (size < DL_FRAME_BYTES)) { return TELEM_ERR_ARG; }

    if (!fields_ok(s)) { return TELEM_ERR_RANGE; }

    put(&w, s->state, DL_STATE_SHIFT, DL_STATE_BITS);
    put(&w, s->error, DL_ERROR_SHIFT, DL_ERROR_BITS);
    put(&w, s->wifi_loss, DL_WIFI_LOSS_SHIFT, DL_WIFI_LOSS_BITS);
    put(&w, s->low_batt, DL_LOW_BATT_SHIFT, DL_LOW_BATT_BITS);
    put(&w, s->drop, DL_DROP_SHIFT, DL_DROP_BITS);
    put(&w, s->hump, DL_HUMP_SHIFT, DL_HUMP_BITS);
    put(&w, s->speed, DL_SPEED_SHIFT, DL_SPEED_BITS);
    put(&w, s->bc_dir, DL_BC_DIR_SHIFT, DL_BC_DIR_BITS);
    put(&w, s->bc_err, DL_BC_ERR_SHIFT, DL_BC_ERR_BITS);
    put(&w, s->line, DL_LINE_SHIFT, DL_LINE_BITS);
    put(&w, s->dist, DL_DIST_SHIFT, DL_DIST_BITS);
    put(&w, s->dir, DL_DIR_SHIFT, DL_DIR_BITS);
    put(&w, s->hump_h, DL_HUMP_H_SHIFT, DL_HUMP_H_BITS);
    put(&w, angle_enc(s->tilt), DL_TILT_SHIFT, DL_TILT_BITS);
    put(&w, angle_enc(s->roll), DL_ROLL_SHIFT, DL_ROLL_BITS);
    put(&w, s->seq, DL_SEQ_SHIFT, DL_SEQ_BITS);

    out[0] = (uint8_t)DL_VERSION;
    for (i = 0u; i < DL_PAYLOAD_BYTES; i++) {
        out[1u + i] = (uint8_t)(w >> (DL_BITS_PER_BYTE * i));      /* little-endian */
    }
    return TELEM_OK;
}

int telemetry_unpack(const uint8_t *in, uint32_t len, telemetry_sample_t *s)
{
    uint64_t w = 0u;
    uint32_t i;

    if ((!in) || (!s) || (len != DL_FRAME_BYTES) || (in[0] != DL_VERSION)) {
        return TELEM_ERR_ARG;
    }
    for (i = 0u; i < DL_PAYLOAD_BYTES; i++) { w |= (uint64_t)in[1u + i] << (DL_BITS_PER_BYTE * i); }

    s->state = (uint8_t)get(w, DL_STATE_SHIFT, DL_STATE_BITS);
    s->error = (uint8_t)get(w, DL_ERROR_SHIFT, DL_ERROR_BITS);
    s->wifi_loss = (uint8_t)get(w, DL_WIFI_LOSS_SHIFT, DL_WIFI_LOSS_BITS);
    s->low_batt = (uint8_t)get(w, DL_LOW_BATT_SHIFT, DL_LOW_BATT_BITS);
    s->drop = (uint8_t)get(w, DL_DROP_SHIFT, DL_DROP_BITS);
    s->hump = (uint8_t)get(w, DL_HUMP_SHIFT, DL_HUMP_BITS);
    s->speed = (uint8_t)get(w, DL_SPEED_SHIFT, DL_SPEED_BITS);
    s->bc_dir = (uint8_t)get(w, DL_BC_DIR_SHIFT, DL_BC_DIR_BITS);
    s->bc_err = (uint8_t)get(w, DL_BC_ERR_SHIFT, DL_BC_ERR_BITS);
    s->line = (uint8_t)get(w, DL_LINE_SHIFT, DL_LINE_BITS);
    s->dist = (uint8_t)get(w, DL_DIST_SHIFT, DL_DIST_BITS);
    s->dir = (uint16_t)get(w, DL_DIR_SHIFT, DL_DIR_BITS);
    s->hump_h = (uint8_t)get(w, DL_HUMP_H_SHIFT, DL_HUMP_H_BITS);
    s->seq = (uint8_t)get(w, DL_SEQ_SHIFT, DL_SEQ_BITS);
    s->tilt = angle_dec(get(w, DL_TILT_SHIFT, DL_TILT_BITS));
    s->roll = angle_dec(get(w, DL_ROLL_SHIFT, DL_ROLL_BITS));
    return TELEM_OK;
}

uint8_t telemetry_cmd_decode(const uint8_t *in, uint32_t len)
{
    if ((!in) || (len != DL_CMD_BYTES)) { return 0u; }
    return ((in[0] >= DL_CMD_START) && (in[0] <= DL_CMD_RESET)) ? in[0] : 0u;
}