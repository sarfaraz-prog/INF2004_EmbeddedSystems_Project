/*
 * test_comms_task.c -- PC test of the connection and sending logic in comms.c.
 *
 * The platform (clock, radio, WiFi join, LED, lwIP lock) is replaced by
 * stubs/fake_port.h, and lwIP's MQTT client by stubs/lwip/apps/mqtt.h (a fake
 * broker that the test controls). So this checks the LOGIC of comms.c (retry
 * timing, send order, ack handling, link-loss detection, WiFi loss, inbox,
 * overflow, subscribe, LED, lock discipline). It does NOT test the real radio
 * or a real broker: that is done on the Pico W (S1-H tests).
 *
 * Each check prints an ID S1-Cnn in the order it runs (S1-C01, S1-C02, ...).
 * If you add a check in the middle, the later IDs move: add new checks at the
 * END so the IDs in TEST_LOG.md stay the same.
 *
 * Build and run from the repo root:
 *   gcc -std=c11 -Wall -Wextra -fsanitize=address,undefined -DCOMMS_HOST_TEST \
 *       -Itesting/subsystem1/stubs -Iapp_program/subsystem1 \
 *       testing/subsystem1/test_comms_task.c \
 *       app_program/subsystem1/trans_queue.c app_program/subsystem1/com_state.c \
 *       app_program/subsystem1/status_led.c app_program/subsystem1/telemetry.c \
 *       -o /tmp/test_comms_task
 *   /tmp/test_comms_task
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fake_port.h"
#include "sample_id.h"
#include "../../app_program/subsystem1/comms.c"

static int fails = 0;
static int checks = 0;
#define CHECK(c, m) do { checks++; \
    if (c) { printf("PASS S1-C%02d %s\n", checks, m); } \
    else { printf("FAIL S1-C%02d %s\n", checks, m); fails++; } } while (0)

/* run the task for ms of fake time, 10 ms at a time */
static void step(uint32_t ms)
{
    for (uint32_t i = 0; i < ms / 10u; i++) {
        fk_now_ms += 10;
        comms_task();
        comms_led_task();
    }
}
static void broker_accept(void) { fk_conn_cb(&fk_client, NULL, MQTT_CONNECT_ACCEPTED); }
static void broker_ack(err_t e) { fk_pub_cb(fk_pub_arg, e); }
static uint32_t pub_ts(void)
{
    uint32_t t = 0;
    (void)sid_get(fk_pub_payload, fk_pub_len, &t);
    return t;
}
static int send_ts(uint32_t ts)
{
    telemetry_sample_t s;
    memset(&s, 0, sizeof s);
    s.state = DL_STATE_LINE_FOLLOWING;
    sid_set(&s, ts);
    return comms_send(&s);
}
static uint16_t queued(void) { comms_stats_t st; comms_get_stats(&st); return st.queued; }

/* let the broker confirm everything that is queued (with a safety limit) */
static void drain_all(void)
{
    for (int i = 0; i < 400 && queued() > 0; i++) { broker_ack(ERR_OK); step(10); }
}

/* connect to the broker: accept, then confirm the subscribe */
static void broker_up(void)
{
    step(20);                 /* let comms start the connect */
    broker_accept();
    step(20);                 /* comms subscribes */
    fk_sub_cb(NULL, ERR_OK);
    step(20);
}

int main(void)
{
    comms_config_t cfg = { "ssid", "pw", "10.0.0.2", 1883, "car1", NULL, NULL };
    comms_stats_t st;

    /* ---- A: before init, then start-up and WiFi retry ---- */
    CHECK(send_ts(1) == COMMS_ERR_NOT_READY, "send before init is refused (NOT_READY)");
    comms_task();
    CHECK(fk_connects == 0 && fk_wifi_joins == 0, "comms_task before init does nothing");
    CHECK(comms_init(&cfg), "init succeeds");
    CHECK(comms_init(&cfg), "a second init is harmless");
    CHECK(fk_wifi_joins == 1, "init starts one WiFi join");
    CHECK(comms_get_state() == COM_DISCONNECTED, "power-up state is DISCONNECTED");
    step(4900);
    CHECK(fk_wifi_joins == 1, "no WiFi retry before 5 s");
    step(200);
    CHECK(fk_wifi_joins == 2, "WiFi retry at 5 s (FR-COM-02)");

    /* ---- B: offline sampling and LED ---- */
    CHECK(send_ts(1000) == COMMS_OK && send_ts(2000) == COMMS_OK && send_ts(3000) == COMMS_OK,
          "three samples accepted while offline");
    step(10);
    CHECK(queued() == 3, "queue holds 3");
    fk_led_changes = 0;
    step(1000);
    CHECK(fk_led_changes >= 3 && fk_led_changes <= 5, "LED blinks at about 2 Hz when DISCONNECTED");
    CHECK(fk_publishes == 0, "nothing published while offline");

    /* ---- C: MQTT connect and 5 s retry ---- */
    fk_link = PORT_LINK_UP;
    step(20);
    CHECK(fk_connects == 1, "MQTT connect starts as soon as WiFi is up");
    step(5100);
    CHECK(fk_disconnects >= 1 && fk_connects == 2, "unanswered attempt is cancelled and retried");
    {
        uint32_t dt = fk_connect_times[1] - fk_connect_times[0];
        printf("     (attempt 1 to attempt 2: %u ms)\n", (unsigned)dt);
        CHECK(dt >= 5000 && dt <= 5100, "retry interval is 5 s (FR-COM-02)");
    }

    /* ---- D: connected, subscribe, oldest first, one in flight ---- */
    broker_accept();
    step(20);
    CHECK(fk_subscribes == 1, "subscribes to the command topic after connect");
    fk_sub_cb(NULL, ERR_OK);
    step(500);
    CHECK(fk_subscribes == 1, "subscribes only once per connection");
    CHECK(fk_publishes == 1 && pub_ts() == 1000, "first publish is the OLDEST message");
    CHECK(strcmp(fk_pub_topic, COMMS_TOPIC_TELEMETRY) == 0, "published on the telemetry topic");
    CHECK(queued() == 3, "message stays queued until the broker confirms");

    /* ---- E: acks drain the queue in order ---- */
    broker_ack(ERR_OK); step(20);
    CHECK(fk_publishes == 2 && pub_ts() == 2000, "second message sent after first ack");
    broker_ack(ERR_OK); step(20);
    CHECK(fk_publishes == 3 && pub_ts() == 3000, "third message sent, order kept");
    CHECK(comms_get_state() == COM_SYNC, "state is SYNC while messages are waiting");
    broker_ack(ERR_OK); step(20);
    comms_get_stats(&st);
    CHECK(queued() == 0 && st.sent_acked == 3, "queue empty, 3 acked");
    CHECK(comms_get_state() == COM_CONNECTED, "state is CONNECTED when the queue is empty");
    fk_led_changes = 0;
    step(1000);
    CHECK(fk_led == 1 && fk_led_changes == 0, "LED solid ON when CONNECTED");

    /* ---- F: a failed ack keeps the message ---- */
    int pubs = fk_publishes;
    send_ts(4000);
    step(20);
    CHECK(fk_publishes == pubs + 1 && pub_ts() == 4000, "live message is published");
    broker_ack(ERR_BUF); step(20);
    CHECK(queued() == 1, "failed ack: message NOT removed (FR-COM-12)");
    CHECK(fk_publishes == pubs + 2 && pub_ts() == 4000, "failed ack: same message sent again");

    /* ---- G: no ack at all = link lost ---- */
    int disc = fk_disconnects;
    int subs = fk_subscribes;
    step(COMMS_ACK_TIMEOUT_MS + 100u);
    CHECK(fk_disconnects == disc + 1 && comms_get_state() == COM_DISCONNECTED,
          "no ack within the ack timeout: link treated as lost, DISCONNECTED (FR-COM-01)");
    CHECK(queued() == 1, "message kept after the link loss");
    broker_up();
    CHECK(fk_subscribes == subs + 1, "subscribes again after the reconnect");
    CHECK(pub_ts() == 4000, "after reconnect the same message is sent again");
    broker_ack(ERR_OK); step(20);
    CHECK(queued() == 0, "message removed only after the ack");

    /* ---- H: a late ack for an OLD message must not remove the current one ---- */
    send_ts(4500);
    step(20);
    {
        uintptr_t cur = (uintptr_t)fk_pub_arg;
        fk_pub_cb((void *)(cur - 1), ERR_OK);    /* a late answer for an older message */
        step(20);
        CHECK(queued() == 1, "late ack for an old message does not remove the current one");
        broker_ack(ERR_OK); step(20);
        CHECK(queued() == 0, "the right ack removes it");
    }

    /* ---- I: WiFi lost, messages kept in order, resubscribe ---- */
    subs = fk_subscribes;
    fk_link = PORT_LINK_DOWN;
    send_ts(5000); send_ts(6000);
    step(20);
    CHECK(comms_get_state() == COM_DISCONNECTED, "WiFi lost: DISCONNECTED");
    CHECK(queued() == 2, "messages kept while WiFi is down");
    fk_link = PORT_LINK_UP;
    broker_up();
    CHECK(fk_subscribes == subs + 1, "subscribes again after WiFi returns");
    CHECK(pub_ts() == 5000, "after reconnect the oldest queued message goes first");
    broker_ack(ERR_OK); step(20);
    CHECK(pub_ts() == 6000, "then the next, order kept");
    broker_ack(ERR_OK); step(20);
    CHECK(comms_get_state() == COM_CONNECTED, "back to CONNECTED when drained");

    /* ---- J: link-loss detection time, worst case (FR-COM-01) ----
     * Connected, queue empty. The broker stops answering at t0. The next
     * sample is made 990 ms later (a sample is made every 1000 ms), so this
     * is the longest wait before the loss can be seen. */
    {
        uint32_t t0 = fk_now_ms;
        step(990);
        send_ts(7000);
        uint32_t guard = 0;
        while (comms_get_state() != COM_DISCONNECTED && guard < 500) {
            step(10);
            guard++;
        }
        uint32_t dt = fk_now_ms - t0;
        printf("     (link loss seen %u ms after the broker stopped answering)\n", (unsigned)dt);
        CHECK(comms_get_state() == COM_DISCONNECTED && dt <= 1000u + COMMS_ACK_TIMEOUT_MS + 100u,
              "link loss detected within 1 s sample + ack timeout, worst case (FR-COM-01)");
    }

    /* ---- K: overflow while disconnected (one sample per 10 ms step) ---- */
    {
        broker_up();
        broker_ack(ERR_OK); step(20);
        CHECK(queued() == 0, "queue empty before the overflow test");
        fk_link = PORT_LINK_DOWN;
        step(20);
        int all_ok = 1;
        for (int i = 0; i < 105; i++) {
            if (send_ts(10000u + (uint32_t)i) != COMMS_OK) { all_ok = 0; }
            step(10);                    /* comms_task moves it from the inbox to the queue */
        }
        comms_get_stats(&st);
        CHECK(all_ok, "105 samples accepted one at a time");
        CHECK(queued() == 100 && st.discarded == 5, "queue holds 100, 5 oldest discarded (FR-COM-09/10/11)");
        fk_link = PORT_LINK_UP;
        broker_up();
        CHECK(pub_ts() == 10005, "after overflow the oldest KEPT message goes first (sample 6)");
        broker_ack(ERR_OK); step(20);
    }

    /* ---- L: commands while sending ---- */
    step(COMMS_CMD_SETTLE_MS + 50u);        /* commands are ignored for 300 ms after a subscribe */
    { uint8_t stop = DL_CMD_STOP;
      inpub_cb(NULL, COMMS_TOPIC_CMD, 1);
      indata_cb(NULL, &stop, 1, MQTT_DATA_FLAG_LAST); }
    CHECK(comms_get_command() == COMMS_CMD_STOP, "command received while sending telemetry");

    /* ---- M: bad arguments ---- */
    comms_config_t bad = { NULL, "pw", "10.0.0.2", 1883, "car1", NULL, NULL };
    CHECK(!comms_init(&bad), "init rejects a missing SSID");
    CHECK(comms_send(NULL) < 0, "send rejects a NULL sample");

    /* ---- N: the inbox between comms_send() and comms_task() ----
     * comms_send() only fills a small inbox; comms_task() empties it.
     * Samples sent faster than comms_task runs are dropped and counted. */
    {
        fk_link = PORT_LINK_UP;
        broker_up();                     /* make sure we are connected */
        drain_all();
        comms_get_stats(&st);
        uint32_t d0 = st.discarded;
        int ok = 0, busy = 0;
        for (int i = 0; i < 20; i++) {   /* no comms_task between these */
            int r = send_ts(20000u + (uint32_t)i);
            if (r == COMMS_OK) { ok++; }
            if (r == COMMS_ERR_BUSY) { busy++; }
        }
        CHECK(ok == 7 && busy == 13, "inbox holds 7: the next 13 samples get BUSY");
        step(10);
        comms_get_stats(&st);
        CHECK(st.discarded - d0 == 13, "the 13 dropped samples are counted as discarded");
        CHECK(queued() == 7, "the 7 accepted samples reached the queue");
        CHECK(pub_ts() == 20000, "and the first one is sent first");
        drain_all();
    }

    /* ---- O: the same inbox when send and task alternate (no loss) ---- */
    {
        comms_get_stats(&st);
        uint32_t d0 = st.discarded;
        int bad_ret = 0;
        for (int i = 0; i < 500; i++) {
            if (send_ts(30000u + (uint32_t)i) != COMMS_OK) { bad_ret++; }
            step(10);
            broker_ack(ERR_OK);          /* the broker keeps up */
        }
        comms_get_stats(&st);
        CHECK(bad_ret == 0 && st.discarded == d0, "500 samples at 1 per task call: none lost");
        drain_all();
    }

    /* ---- P: lwIP lock discipline (matters for the SDK bench build) ---- */
    CHECK(fk_unlocked_calls == 0, "every lwIP call was made with the port lock held");
    CHECK(fk_lock_errors == 0 && fk_lock_depth == 0, "lock and unlock are always paired");

    printf("%d checks, %s (%d failed)\n", checks, fails ? "FAILED" : "ALL PASSED", fails);
    return fails != 0;
}