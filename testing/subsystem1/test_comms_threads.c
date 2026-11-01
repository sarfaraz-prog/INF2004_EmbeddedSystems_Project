/*
 * test_comms_threads.c -- two threads share the inbox and the command ring,
 * like a sending task and the task that runs comms_task() would on the car.
 *
 * Thread A (the "Telemetry task") calls comms_send() and comms_get_command().
 * Thread B (the "WiFi task") calls comms_task() and plays the broker.
 * Checks that nothing is lost, repeated, reordered or corrupted.
 *
 * LIMIT: a PC (x86) orders memory writes strongly, so this proves the index
 * logic under real interleaving but NOT the memory-barrier placement. The
 * barriers (PORT_MB) are checked by reading the code, and on the board.
 *
 * Each check prints an ID S1-Rnn. Add new checks at the END.
 * Build and run from the repo root (see run_pc_tests.sh for the flags):
 *   gcc -std=c11 -Wall -Wextra -pthread -DCOMMS_HOST_TEST -Itesting/subsystem1/stubs \
 *       -Iapp_program/subsystem1 testing/subsystem1/test_comms_threads.c \
 *       app_program/subsystem1/{trans_queue,com_state,status_led,telemetry}.c -o /tmp/t
 */
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include "fake_port.h"
#include "sample_id.h"
#include "../../app_program/subsystem1/comms.c"

#define N_SAMPLES  200000u
#define N_CMDS     100000u

static int fails = 0, checks = 0;
#define CHECK(c, m) do { checks++; \
    if (c) { printf("PASS S1-R%02d %s\n", checks, m); } \
    else { printf("FAIL S1-R%02d %s\n", checks, m); fails++; } } while (0)

static volatile int stop_b = 0;
static volatile uint32_t pub_count = 0;
static uint32_t pub_bad_order = 0, pub_bad_text = 0, last_ts = 0, pub_gaps = 0;
static uint32_t cmds_pushed = 0;
static volatile int push_cmds_done = 0;

/* Thread B: the WiFi task. Runs comms_task(), answers every publish at once,
 * and pushes commands in like the lwIP callback would. */
static void *wifi_task(void *arg)
{
    (void)arg;
    while (!stop_b) {
        fk_now_ms++;
        comms_task();
        /* the broker confirms the message that was just published */
        if (fk_publishes > 0 && fk_pub_cb != NULL && fk_pub_arg != NULL) {
            uint32_t ts = 0;
            if (!sid_get(fk_pub_payload, fk_pub_len, &ts)) {
                pub_bad_text++;                      /* not a valid 9-byte frame */
            }
            mqtt_request_cb_t cb = fk_pub_cb;
            void *a = fk_pub_arg;
            fk_pub_arg = NULL;                       /* answer each publish once */
            pub_count++;
            if (ts <= last_ts && pub_count > 1) { pub_bad_order++; }
            if (ts != last_ts + 1u) { pub_gaps++; }          /* a missing sample */
            last_ts = ts;
            cb(a, ERR_OK);
        }
        /* commands: push only when there is room (a full ring would drop, which is tested elsewhere) */
        if (cmds_pushed < N_CMDS) {
            /* commands are only accepted once the 300 ms settle time after subscribing is over */
            if (!g_cmd_gate || (int32_t)(comms_now_ms() - g_cmd_open_at) < 0) { continue; }
            uint8_t next = (uint8_t)((g_cmd_head + 1u) % CMD_RING_SIZE);
            if (next != g_cmd_tail) {
                uint8_t t = (uint8_t)(DL_CMD_START + cmds_pushed % 3u);   /* 1,2,3 */
                inpub_cb(NULL, COMMS_TOPIC_CMD, 1u);
                indata_cb(NULL, &t, 1u, MQTT_DATA_FLAG_LAST);
                cmds_pushed++;
            }
        } else {
            push_cmds_done = 1;
        }
    }
    return NULL;
}

int main(void)
{
    comms_config_t cfg = { "ssid", "pw", "10.0.0.2", 1883, "car1", NULL, NULL };
    pthread_t b;
    uint32_t sent = 0, busy = 0, cmds_got = 0, cmd_bad_order = 0;
    comms_stats_t st;

    comms_init(&cfg);
    fk_link = PORT_LINK_UP;
    /* bring the connection up from this thread before the second thread starts */
    for (int i = 0; i < 5; i++) { fk_now_ms += 10; comms_task(); }
    fk_conn_cb(&fk_client, NULL, MQTT_CONNECT_ACCEPTED);
    for (int i = 0; i < 5; i++) { fk_now_ms += 10; comms_task(); }
    if (fk_sub_cb) { fk_sub_cb(NULL, ERR_OK); }
    fk_publishes = 0; fk_pub_arg = NULL;

    pthread_create(&b, NULL, wifi_task, NULL);

    /* Thread A: send N samples as fast as possible, read commands as fast as possible */
    comms_cmd_t expect_next = COMMS_CMD_START;
    while (sent < N_SAMPLES || cmds_got < N_CMDS) {
        /* send only while fewer than 40 are waiting (sent minus published): this test is
         * about the inbox, not about queue overflow */
        if (sent < N_SAMPLES && (sent - pub_count) < 40u) {
            telemetry_sample_t s;
            memset(&s, 0, sizeof s);
            s.state = DL_STATE_LINE_FOLLOWING;
            sid_set(&s, sent + 1u);
            int r = comms_send(&s);
            if (r == COMMS_OK) { sent++; }
            else if (r == COMMS_ERR_BUSY) { busy++; s_inbox_drops--; /* retry: not a real loss */ }
        }
        comms_cmd_t c = comms_get_command();
        if (c != COMMS_CMD_NONE) {
            if (c != expect_next) { cmd_bad_order++; }
            expect_next = (c == COMMS_CMD_START) ? COMMS_CMD_STOP :
                          (c == COMMS_CMD_STOP) ? COMMS_CMD_RESET : COMMS_CMD_START;
            cmds_got++;
        }
        if (busy > 50u * N_SAMPLES) { break; }       /* safety */
    }
    /* wait for the last samples to be published */
    for (int i = 0; i < 2000000 && pub_count < N_SAMPLES; i++) { sched_yield(); }
    stop_b = 1;
    pthread_join(b, NULL);
    comms_get_stats(&st);

    printf("     (sent %u, published %u, inbox was full %u times, commands %u/%u)\n",
           (unsigned)sent, (unsigned)pub_count, (unsigned)busy, (unsigned)cmds_got, (unsigned)cmds_pushed);
    CHECK(sent == N_SAMPLES, "200000 samples accepted by comms_send");
    CHECK(pub_count == N_SAMPLES, "all 200000 reached the broker (none lost)");
    CHECK(pub_bad_order == 0 && pub_gaps == 0, "in strict order, no gaps (none repeated, reordered or missing)");
    CHECK(pub_bad_text == 0, "every frame valid (9 bytes, right version) (none corrupted)");
    CHECK(st.discarded == 0, "nothing discarded in the queue");
    CHECK(cmds_got == N_CMDS && cmd_bad_order == 0, "100000 commands received, in order, none lost");

    printf("%d checks, %s (%d failed)\n", checks, fails ? "FAILED" : "ALL PASSED", fails);
    return fails != 0;
}