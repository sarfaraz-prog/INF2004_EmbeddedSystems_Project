/* test_telemetry.c -- PC test for the binary frame. Build:
 * gcc -std=c11 -Wall -Wextra -Wpedantic -I app_program app_program/telemetry.c testing/subsystem1/test_telemetry.c */
#include <stdio.h>
#include <string.h>
#include "telemetry.h"

static int g_fail = 0;
#define CHECK(id, cond, desc) do { if (cond) printf("[TEST] %s PASS  %s\n", id, desc); \
    else { printf("[TEST] %s FAIL  %s (line %d)\n", id, desc, __LINE__); g_fail++; } } while (0)

/* field by field: the struct has padding bytes, so memcmp would be wrong */
static int same(const telemetry_sample_t *a, const telemetry_sample_t *b)
{
    return a->state == b->state && a->error == b->error && a->wifi_loss == b->wifi_loss &&
           a->low_batt == b->low_batt && a->drop == b->drop && a->hump == b->hump &&
           a->speed == b->speed && a->bc_dir == b->bc_dir && a->bc_err == b->bc_err &&
           a->line == b->line && a->dist == b->dist && a->dir == b->dir &&
           a->hump_h == b->hump_h && a->tilt == b->tilt && a->roll == b->roll && a->seq == b->seq;
}

static int roundtrip(const telemetry_sample_t *a)
{
    uint8_t f[DL_FRAME_BYTES]; telemetry_sample_t b;
    memset(&b, 0xAA, sizeof b);
    return telemetry_pack(a, f, sizeof f) == TELEM_OK &&
           telemetry_unpack(f, sizeof f, &b) == TELEM_OK && same(a, &b);
}

int main(void)
{
    telemetry_sample_t lo, hi, x, y;
    uint8_t f[DL_FRAME_BYTES + 4];
    /* known answers from an independent Python encoder */
    static const uint8_t A[9] = {0x01,0x01,0x19,0x00,0xB5,0x00,0xD0,0xA2,0x05};
    static const uint8_t B[9] = {0x01,0xFF,0xBE,0xF7,0xCF,0x02,0xA2,0x05,0xF0};
    uint8_t c;

    memset(&lo, 0, sizeof lo);
    hi = (telemetry_sample_t){7,1,1,1,1,1,62,2,1,3,62,359,128,180,-180,15};
    CHECK("S1-B01", roundtrip(&lo), "all-minimum values round trip");
    CHECK("S1-B02", roundtrip(&hi), "all-maximum values round trip");

    x = (telemetry_sample_t){7,1,1,1,1,1,63,3,1,3,63,511,255,TELEM_ANGLE_INVALID,TELEM_ANGLE_INVALID,15};
    CHECK("S1-B03", roundtrip(&x), "invalid codes round trip");

    x = (telemetry_sample_t){1,0,0,0,0,0,25,0,0,0,32,90,0,0,0,0};
    CHECK("S1-B04", telemetry_pack(&x, f, sizeof f) == 0 && memcmp(f, A, 9) == 0, "known frame A matches");
    CHECK("S1-B05", telemetry_pack(&hi, f, sizeof f) == 0 && memcmp(f, B, 9) == 0, "known frame B matches");

    /* one step beyond each limit must be refused */
    y = hi; y.state = 8;     CHECK("S1-B06", telemetry_pack(&y, f, 9) == TELEM_ERR_RANGE, "state 8 rejected");
    y = hi; y.speed = 64;    CHECK("S1-B07", telemetry_pack(&y, f, 9) == TELEM_ERR_RANGE, "speed 64 rejected");
    y = hi; y.dist = 64;     CHECK("S1-B08", telemetry_pack(&y, f, 9) == TELEM_ERR_RANGE, "distance 64 rejected");
    y = hi; y.dir = 360;     CHECK("S1-B09", telemetry_pack(&y, f, 9) == TELEM_ERR_RANGE, "direction 360 rejected");
    y = hi; y.hump_h = 129;  CHECK("S1-B10", telemetry_pack(&y, f, 9) == TELEM_ERR_RANGE, "hump height 129 rejected");
    y = hi; y.tilt = 181;    CHECK("S1-B11", telemetry_pack(&y, f, 9) == TELEM_ERR_RANGE, "tilt 181 rejected");
    y = hi; y.roll = -181;   CHECK("S1-B12", telemetry_pack(&y, f, 9) == TELEM_ERR_RANGE, "roll -181 rejected");
    y = hi; y.seq = 16;      CHECK("S1-B13", telemetry_pack(&y, f, 9) == TELEM_ERR_RANGE, "sequence 16 rejected");
    y = hi; y.error = 2;     CHECK("S1-B14", telemetry_pack(&y, f, 9) == TELEM_ERR_RANGE, "1-bit flag 2 rejected");

    /* fields must not leak into each other: set one field at a time */
    {
        int ok = 1;
        y = lo; y.dir = 359;  telemetry_pack(&y, f, 9); { telemetry_sample_t z; telemetry_unpack(f, 9, &z);
            ok &= (z.dir == 359 && z.dist == 0 && z.hump_h == 0 && z.speed == 0); }
        y = lo; y.hump_h = 128; telemetry_pack(&y, f, 9); { telemetry_sample_t z; telemetry_unpack(f, 9, &z);
            ok &= (z.hump_h == 128 && z.dir == 0 && z.tilt == 0 + 0 && z.seq == 0); }
        CHECK("S1-B15", ok, "one field set does not change its neighbours");
    }

    CHECK("S1-B16", telemetry_pack(NULL, f, 9) == TELEM_ERR_ARG && telemetry_pack(&lo, f, 8) == TELEM_ERR_ARG,
          "NULL or 8-byte buffer rejected");
    memcpy(f, A, 9); f[0] = 2;
    CHECK("S1-B17", telemetry_unpack(f, 9, &y) == TELEM_ERR_ARG, "wrong version byte rejected");
    CHECK("S1-B18", telemetry_unpack(A, 8, &y) == TELEM_ERR_ARG && telemetry_unpack(A, 10, &y) == TELEM_ERR_ARG,
          "wrong frame length rejected");

    c = DL_CMD_STOP;
    CHECK("S1-B19", telemetry_cmd_decode(&c, 1) == DL_CMD_STOP, "command byte 2 = STOP");
    c = 0; CHECK("S1-B20", telemetry_cmd_decode(&c, 1) == 0, "command byte 0 rejected");
    c = 4; CHECK("S1-B21", telemetry_cmd_decode(&c, 1) == 0, "command byte 4 rejected");
    CHECK("S1-B22", telemetry_cmd_decode(A, 2) == 0 && telemetry_cmd_decode(NULL, 1) == 0, "wrong length or NULL rejected");

    printf(g_fail ? "SOME FAILED (%d)\n" : "ALL PASS\n", g_fail);
    return g_fail ? 1 : 0;
}
