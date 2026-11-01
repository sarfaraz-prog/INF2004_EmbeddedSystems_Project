/*
 * data_layout.h
 * One shared layout for the MQTT telemetry frame and command (change
 * requirement: Minimum-Bit Data Representation). Plain C, constants only.
 * Every buddy includes this file; nobody invents their own bit positions.
 *
 * Frame on the wire = 1 version byte + 8 payload bytes = 9 bytes.
 * The payload is one 64-bit word sent little-endian (byte 0 = bits 0-7).
 * Bit 0 is the least significant bit.
 *
 *   bits   field          size  unit / step           valid     invalid
 *   0-2    state            3   code 0-7              0-7       -
 *   3      error fault      1   0 / 1                 -         -
 *   4      Wi-Fi loss       1   0 / 1 (status only)   -         -
 *   5      low battery      1   0 / 1 (status only)   -         -
 *   6      drop flag        1   0 / 1                 -         -
 *   7      hump detect      1   0 / 1                 -         -
 *   8-13   speed            6   cm/s, step 1          0-62      63
 *   14-15  barcode dir      2   code 0-3              0-2       3 (spare)
 *   16     barcode error    1   0 / 1                 -         -
 *   17-18  line position    2   code 0-3              0-3       -
 *   19-24  distance         6   cm, step 1            0-62      63 (no echo)
 *   25-33  direction        9   degrees, step 1       0-359     511
 *   34-41  hump height      8   mm, step 1            0-128     255   (placeholder)
 *   42-50  tilt             9   degrees + 180         -180..180 511
 *   51-59  roll             9   degrees + 180         -180..180 511
 *   60-63  sequence         4   counts 0-15, wraps    0-15      -
 *                          --
 *                          64 bits (57 field bits + 7 spare used for the
 *                          Wi-Fi loss, low-battery, drop flag and sequence)
 *
 * Values marked "placeholder" are decided again after integration.
 * Wi-Fi loss and low battery are status bits only: they never cause
 * FAULT_STOP (Week 6: link loss never faults).
 * Battery voltage, distance travelled and the timestamp are not sent.
 */
#ifndef DATA_LAYOUT_H
#define DATA_LAYOUT_H

#include <stdint.h>

#define DL_VERSION          1u     /* first byte of every frame */
#define DL_PAYLOAD_BYTES    8u
#define DL_FRAME_BYTES      9u     /* version byte + payload */

#define DL_BITS_PER_BYTE    8u

/* A mask of `bits` ones (typed inline function, not a macro). */
static inline uint64_t dl_mask(uint32_t bits)
{
    return (UINT64_C(1) << bits) - 1u;
}

/* ---- field positions and sizes (shift = lowest bit, bits = width) ---- */
#define DL_STATE_SHIFT        0u
#define DL_STATE_BITS         3u
#define DL_ERROR_SHIFT        3u
#define DL_ERROR_BITS         1u
#define DL_WIFI_LOSS_SHIFT    4u
#define DL_WIFI_LOSS_BITS     1u
#define DL_LOW_BATT_SHIFT     5u
#define DL_LOW_BATT_BITS      1u
#define DL_DROP_SHIFT         6u
#define DL_DROP_BITS          1u
#define DL_HUMP_SHIFT         7u
#define DL_HUMP_BITS          1u
#define DL_SPEED_SHIFT        8u
#define DL_SPEED_BITS         6u
#define DL_BC_DIR_SHIFT       14u
#define DL_BC_DIR_BITS        2u
#define DL_BC_ERR_SHIFT       16u
#define DL_BC_ERR_BITS        1u
#define DL_LINE_SHIFT         17u
#define DL_LINE_BITS          2u
#define DL_DIST_SHIFT         19u
#define DL_DIST_BITS          6u
#define DL_DIR_SHIFT          25u
#define DL_DIR_BITS           9u
#define DL_HUMP_H_SHIFT       34u
#define DL_HUMP_H_BITS        8u
#define DL_TILT_SHIFT         42u
#define DL_TILT_BITS          9u
#define DL_ROLL_SHIFT         51u
#define DL_ROLL_BITS          9u
#define DL_SEQ_SHIFT          60u
#define DL_SEQ_BITS           4u

#define DL_TOTAL_BITS         64u
#define DL_FIELD_BITS         57u   /* bits of data fields only (no status/seq) */

/* ---- value limits and invalid codes ---- */
#define DL_SPEED_MAX_CMS      62u
#define DL_SPEED_INVALID      63u
#define DL_DIST_MAX_CM        62u
#define DL_DIST_INVALID       63u   /* no echo / unknown */
#define DL_DIR_MAX_DEG        359u
#define DL_DIR_INVALID        511u
#define DL_HUMP_H_MAX_MM      128u  /* placeholder until the ramp is measured */
#define DL_HUMP_H_INVALID     255u
#define DL_TILT_OFFSET        180   /* stored value = degrees + 180 */
#define DL_TILT_MIN_DEG       (-180)
#define DL_TILT_MAX_DEG       180
#define DL_TILT_INVALID       511u
#define DL_SEQ_MAX            15u
#define DL_STATE_MAX           7u   /* 8 states in 3 bits */
#define DL_BC_DIR_MAX          3u   /* 2 bits (3 is spare) */
#define DL_LINE_MAX            3u   /* 2 bits */

/* ---- state codes (the 8 Week 6 navigation states) ---- */
typedef enum
{
    DL_STATE_CALIBRATE = 0,
    DL_STATE_LINE_FOLLOWING,
    DL_STATE_LINE_LOST,
    DL_STATE_BARCODE_EXECUTION,
    DL_STATE_LINE_SEARCH,
    DL_STATE_OBSTACLE_AVOIDANCE,
    DL_STATE_BARCODE_RESCAN,
    DL_STATE_FAULT_STOP            /* = 7 */
} dl_state_t;

/* ---- barcode direction codes (2 bits) ---- */
typedef enum
{
    DL_BC_NONE = 0,
    DL_BC_LEFT = 1,
    DL_BC_RIGHT = 2                /* 3 is spare */
} dl_bc_dir_t;

/* ---- line position codes (2 bits, same as ir_line_position_t) ---- */
typedef enum
{
    DL_LINE_CENTRED = 0,
    DL_LINE_DRIFT_RIGHT = 1,
    DL_LINE_DRIFT_LEFT = 2,
    DL_LINE_CROSSING = 3
} dl_line_t;

/* ---- command sent to robocar/car1/cmd: one byte (proposed) ---- */
#define DL_CMD_BYTES          1u
#define DL_CMD_START          1u
#define DL_CMD_STOP           2u
#define DL_CMD_RESET          3u

/* ---- compile-time checks: fields touch, none overlap, total is 64 ---- */
#define DL_NEXT(prev) (prev##_SHIFT + prev##_BITS)

_Static_assert(DL_ERROR_SHIFT   == DL_NEXT(DL_STATE),     "error follows state");
_Static_assert(DL_WIFI_LOSS_SHIFT == DL_NEXT(DL_ERROR),   "wifi loss follows error");
_Static_assert(DL_LOW_BATT_SHIFT == DL_NEXT(DL_WIFI_LOSS), "low battery follows wifi loss");
_Static_assert(DL_DROP_SHIFT    == DL_NEXT(DL_LOW_BATT),  "drop follows low battery");
_Static_assert(DL_HUMP_SHIFT    == DL_NEXT(DL_DROP),      "hump follows drop");
_Static_assert(DL_SPEED_SHIFT   == DL_NEXT(DL_HUMP),      "speed follows hump");
_Static_assert(DL_BC_DIR_SHIFT  == DL_NEXT(DL_SPEED),     "barcode dir follows speed");
_Static_assert(DL_BC_ERR_SHIFT  == DL_NEXT(DL_BC_DIR),    "barcode err follows dir");
_Static_assert(DL_LINE_SHIFT    == DL_NEXT(DL_BC_ERR),    "line follows barcode err");
_Static_assert(DL_DIST_SHIFT    == DL_NEXT(DL_LINE),      "distance follows line");
_Static_assert(DL_DIR_SHIFT     == DL_NEXT(DL_DIST),      "direction follows distance");
_Static_assert(DL_HUMP_H_SHIFT  == DL_NEXT(DL_DIR),       "hump height follows direction");
_Static_assert(DL_TILT_SHIFT    == DL_NEXT(DL_HUMP_H),    "tilt follows hump height");
_Static_assert(DL_ROLL_SHIFT    == DL_NEXT(DL_TILT),      "roll follows tilt");
_Static_assert(DL_SEQ_SHIFT     == DL_NEXT(DL_ROLL),      "sequence follows roll");
_Static_assert(DL_NEXT(DL_SEQ)  == DL_TOTAL_BITS,         "frame is exactly 64 bits");
_Static_assert(DL_TOTAL_BITS == 8u * DL_PAYLOAD_BYTES,    "payload size matches bits");
_Static_assert(DL_FRAME_BYTES == DL_PAYLOAD_BYTES + 1u,   "frame = version + payload");

/* the invalid codes and limits must fit the field widths */
_Static_assert(DL_SPEED_INVALID  <= (1u << DL_SPEED_BITS) - 1u,  "speed invalid fits");
_Static_assert(DL_DIST_INVALID   <= (1u << DL_DIST_BITS) - 1u,   "distance invalid fits");
_Static_assert(DL_DIR_INVALID    <= (1u << DL_DIR_BITS) - 1u,    "direction invalid fits");
_Static_assert(DL_HUMP_H_INVALID <= (1u << DL_HUMP_H_BITS) - 1u, "hump height invalid fits");
_Static_assert(DL_TILT_INVALID   <= (1u << DL_TILT_BITS) - 1u,   "tilt invalid fits");
_Static_assert((DL_TILT_MAX_DEG + DL_TILT_OFFSET) < (int)DL_TILT_INVALID, "tilt range below invalid");
_Static_assert(DL_STATE_FAULT_STOP == 7, "8 states fit 3 bits");

#endif /* DATA_LAYOUT_H */