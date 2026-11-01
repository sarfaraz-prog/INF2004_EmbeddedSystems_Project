/*
 * test_scenario.c  
 */
#include <stdio.h>
#include <string.h>

#include "trans_queue.h"
#include "com_state.h"
#include "status_led.h"
#include "telemetry.h"

static int g_fail = 0;

#define CHECK(id, cond, desc)                                              \
    do {                                                                   \
        if (cond) {                                                        \
            printf("[TEST] %s PASS  %s\n", (id), (desc));                  \
        } else {                                                           \
            printf("[TEST] %s FAIL  %s   (line %d)\n", (id), (desc), __LINE__); \
            g_fail++;                                                      \
        }                                                                  \
    } while (0)

/* What the pretend broker received, in arrival order (the sample number of each). */
#define BROKER_MAX 400
static unsigned long g_broker[BROKER_MAX];
static unsigned g_broker_n = 0;

static uint32_t g_now_ms = 0;
static unsigned long g_next_ts = 1;

/* One telemetry sample is taken: format it and put it in the queue. */
static void take_sample(void)
{
    telemetry_sample_t s;
    uint8_t buf[DL_FRAME_BYTES];

    memset(&s, 0, sizeof s);
    s.state = DL_STATE_LINE_FOLLOWING;
    s.speed = (uint8_t)(g_next_ts / 63u);      /* the frame has no timestamp, so the */
    s.dist  = (uint8_t)(g_next_ts % 63u);      /* sample number rides in speed + dist */
    g_next_ts++;

    if (telemetry_pack(&s, buf, sizeof buf) == TELEM_OK) {
        (void)tq_push(buf, (uint16_t)sizeof buf);
    }
}

/* One control tick: update the state, then send one message if the
 * broker is reachable (send, then acknowledge). */
static com_state_t tick(bool wifi, bool mqtt)
{
    com_inputs_t in;
    com_state_t st;
    tq_msg_t m;

    g_now_ms += 100u;
    in.wifi_up = wifi;
    in.mqtt_up = mqtt;
    in.queue_empty = (tq_count() == 0u);
    st = com_update(&in);

    if (wifi && mqtt && tq_peek_oldest(&m) == TQ_OK) {
        telemetry_sample_t d;
        if (telemetry_unpack(m.data, m.len, &d) == TELEM_OK && g_broker_n < BROKER_MAX) {
            g_broker[g_broker_n++] = (unsigned long)d.speed * 63u + d.dist;
        }
        (void)tq_ack_oldest(m.seq);
    }
    return st;
}

static bool broker_in_order(void)
{
    unsigned i;
    for (i = 1; i < g_broker_n; i++) {
        if (g_broker[i] <= g_broker[i - 1]) {
            return false;
        }
    }
    return true;
}

int main(void)
{
    unsigned i;
    com_state_t st;

    tq_init();
    com_init();

    /* Phase 1: power-up, no link. */
    st = tick(false, false);
    CHECK("S1-S01", st == COM_DISCONNECTED, "power-up with no link: DISCONNECTED");
    CHECK("S1-S02", status_led_level(st, 0u) == true &&
                    status_led_level(st, 300u) == false,
          "DISCONNECTED: LED is blinking (on then off)");

    for (i = 0; i < 3; i++) {
        take_sample();
        st = tick(false, false);
    }
    CHECK("S1-S03", tq_count() == 3u && g_broker_n == 0u,
          "3 samples while offline: all stored, none sent");

    /* Phase 2: link comes back, backlog is sent. */
    st = tick(true, true);
    CHECK("S1-S04", st == COM_SYNC, "link back with backlog: SYNC");
    CHECK("S1-S05", status_led_level(st, 300u) == true,
          "SYNC: LED stays on");

    for (i = 0; i < 10 && st != COM_CONNECTED; i++) {
        st = tick(true, true);
    }
    CHECK("S1-S06", st == COM_CONNECTED && tq_count() == 0u,
          "backlog drained: CONNECTED, queue empty");
    CHECK("S1-S07", g_broker_n == 3u && g_broker[0] == 1u &&
                    g_broker[1] == 2u && g_broker[2] == 3u,
          "broker received samples 1,2,3 in order");

    /* Phase 3: link drops again. */
    st = tick(false, false);
    CHECK("S1-S08", st == COM_DISCONNECTED, "link lost: back to DISCONNECTED");

    /* Phase 4: long outage, queue overflows (105 samples, capacity 100). */
    for (i = 0; i < 105; i++) {
        take_sample();
        st = tick(false, false);
    }
    CHECK("S1-S09", tq_count() == TQ_CAPACITY && tq_discarded() == 5u,
          "105 offline samples: 100 kept, 5 discarded");

    g_broker_n = 0;
    st = tick(true, true);
    CHECK("S1-S10", st == COM_SYNC, "reconnect after overflow: SYNC");
    for (i = 0; i < 200 && st != COM_CONNECTED; i++) {
        st = tick(true, true);
    }
    CHECK("S1-S11", st == COM_CONNECTED && tq_count() == 0u,
          "overflow backlog drained: CONNECTED");
    CHECK("S1-S12", g_broker_n == 100u && g_broker[0] == 9u &&
                    g_broker[99] == 108u,
          "broker got the newest 100 (ts 9..108); oldest 5 were dropped");
    CHECK("S1-S13", broker_in_order(), "broker order is increasing");
    CHECK("S1-S14", tq_discarded() == 0u,
          "discard counter reset after full transmission (FR-COM-12)");

    if (g_fail == 0) {
        printf("ALL TESTS PASSED (0 failed)\n");
        return 0;
    }
    printf("%d TEST(S) FAILED\n", g_fail);
    return 1;
}