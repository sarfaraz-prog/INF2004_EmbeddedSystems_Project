/* test_comms_frame.c -- what comms_send() adds to the frame: sequence, drop flag, Wi-Fi loss flag. */
#include <stdio.h>
#include "fake_port.h"
#include "sample_id.h"
#include "../../app_program/subsystem1/comms.c"

static int fails = 0, checks = 0;
#define CHECK(c, m) do { checks++; if (c) printf("PASS S1-F%02d %s\n", checks, m); \
    else { printf("FAIL S1-F%02d %s\n", checks, m); fails++; } } while (0)

static telemetry_sample_t last(void)
{
    telemetry_sample_t d;
    uint8_t t = s_in_tail;
    (void)telemetry_unpack(s_inbox[(uint8_t)((s_in_head + INBOX_N - 1u) % INBOX_N)].data, DL_FRAME_BYTES, &d);
    (void)t;
    return d;
}

int main(void)
{
    comms_config_t cfg = { "ssid", "pw", "10.0.0.2", 1883, "car1", NULL, NULL };
    telemetry_sample_t s = {0}, d;
    int i;

    comms_init(&cfg);                       /* link stays down: state DISCONNECTED */
    s.seq = 9; s.drop = 1; s.wifi_loss = 0; /* caller values must be ignored */
    CHECK(comms_send(&s) == COMMS_OK, "send accepted");
    d = last();
    CHECK(d.seq == 0, "first frame has sequence 0 (caller's 9 ignored)");
    CHECK(d.wifi_loss == 1, "Wi-Fi loss flag set while DISCONNECTED");
    CHECK(d.drop == 0, "drop flag 0 (caller's 1 ignored, nothing dropped)");
    s_in_tail = s_in_head;                  /* empty the inbox */
    for (i = 1; i < 20; i++) {
        s.seq = 0; (void)comms_send(&s); d = last();
        if (d.seq != (uint8_t)(i & 15)) { break; }
        s_in_tail = s_in_head;
    }
    CHECK(i == 20, "sequence counts 0-15 and wraps (20 frames checked)");
    s_snap_discarded = 3;                   /* pretend 3 messages were dropped */
    (void)comms_send(&s); d = last(); s_in_tail = s_in_head;
    CHECK(d.drop == 1, "drop flag set on the first frame after a drop");
    (void)comms_send(&s); d = last(); s_in_tail = s_in_head;
    CHECK(d.drop == 0, "drop flag clears again when nothing new was dropped");
    s.speed = 64;
    CHECK(comms_send(&s) == TELEM_ERR_RANGE, "out-of-range value refused, not sent");
    CHECK(s_in_tail == s_in_head, "refused sample did not enter the inbox");
    printf("%d checks, %s (%d failed)\n", checks, fails ? "FAILED" : "ALL PASSED", fails);
    return fails != 0;
}