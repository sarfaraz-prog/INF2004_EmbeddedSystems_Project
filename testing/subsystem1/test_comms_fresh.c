/* test_comms_fresh.c -- Version C cases: stale/retained command after reconnect,
 * Wi-Fi down vs broker down. PC test with a fake broker (see test_comms_task.c). */
#include <stdio.h>
#include <string.h>
#include "fake_port.h"
#include "sample_id.h"
#include "../../app_program/subsystem1/comms.c"

static int fails = 0, checks = 0;
#define CHECK(c, m) do { checks++; if (c) printf("PASS S1-V%02d %s\n", checks, m); \
    else { printf("FAIL S1-V%02d %s\n", checks, m); fails++; } } while (0)

static void step(uint32_t ms)
{ for (uint32_t i = 0; i < ms / 10u; i++) { fk_now_ms += 10; comms_task(); } }
static void cmd(uint8_t v)
{ inpub_cb(NULL, COMMS_TOPIC_CMD, 1); indata_cb(NULL, &v, 1, MQTT_DATA_FLAG_LAST); }
static telemetry_sample_t sent_flags(void)
{
    telemetry_sample_t s = {0}, d;
    s_in_tail = s_in_head;                       /* empty the inbox first */
    (void)comms_send(&s);
    (void)telemetry_unpack(s_inbox[(uint8_t)((s_in_head + INBOX_N - 1u) % INBOX_N)].data, DL_FRAME_BYTES, &d);
    s_in_tail = s_in_head;
    return d;
}
static void mqtt_up_and_subscribed(void)
{
    step(20); fk_conn_cb(&fk_client, NULL, MQTT_CONNECT_ACCEPTED); step(20); fk_sub_cb(NULL, ERR_OK); step(20);
}

int main(void)
{
    comms_config_t cfg = { "ssid", "pw", "10.0.0.2", 1883, "car1", NULL, NULL };
    telemetry_sample_t d;
    uint32_t stale;

    comms_init(&cfg);
    CHECK(sent_flags().wifi_loss == 1, "Wi-Fi down at start: Wi-Fi loss flag = 1");
    fk_link = PORT_LINK_UP; step(50);
    CHECK(sent_flags().wifi_loss == 0, "Wi-Fi up (broker not yet reached): Wi-Fi loss flag = 0");

    /* before the subscribe is confirmed, a command is ignored */
    stale = g_cmds_stale; cmd(DL_CMD_START);
    CHECK(comms_get_command() == COMMS_CMD_NONE && g_cmds_stale == stale + 1, "command before subscribe ignored (counted stale)");

    step(20); fk_conn_cb(&fk_client, NULL, MQTT_CONNECT_ACCEPTED); step(20); fk_sub_cb(NULL, ERR_OK); step(20);
    CHECK(comms_get_command() == COMMS_CMD_NONE, "(nothing queued yet)");
    /* retained START arrives right after the subscribe */
    stale = g_cmds_stale; cmd(DL_CMD_START);
    CHECK(comms_get_command() == COMMS_CMD_NONE && g_cmds_stale == stale + 1, "retained START right after subscribe ignored");
    step(COMMS_CMD_SETTLE_MS);
    cmd(DL_CMD_STOP);
    CHECK(comms_get_command() == COMMS_CMD_STOP, "fresh command after the settle time accepted");

    /* broker goes away while Wi-Fi stays up */
    fk_pub_arg = NULL;
    mqtt_client_t *c = &fk_client; (void)c;
    fk_conn_cb(&fk_client, NULL, 3 /* connection lost */);
    step(20);
    d = sent_flags();
    CHECK(d.wifi_loss == 0 && comms_get_state() == COM_DISCONNECTED, "broker down, Wi-Fi up: state DISCONNECTED but Wi-Fi loss flag = 0");
    stale = g_cmds_stale; cmd(DL_CMD_RESET);
    CHECK(comms_get_command() == COMMS_CMD_NONE && g_cmds_stale == stale + 1, "command while broker is down ignored");

    /* reconnect: the broker replays the retained START again */
    step(5100);                                  /* next connect attempt */
    mqtt_up_and_subscribed();
    stale = g_cmds_stale; cmd(DL_CMD_START);
    CHECK(comms_get_command() == COMMS_CMD_NONE && g_cmds_stale == stale + 1, "stale START replayed after reconnect ignored");
    step(COMMS_CMD_SETTLE_MS); cmd(DL_CMD_START);
    CHECK(comms_get_command() == COMMS_CMD_START, "a new START after the settle time works");

    /* Wi-Fi goes down */
    fk_link = PORT_LINK_DOWN; step(50);
    CHECK(sent_flags().wifi_loss == 1, "Wi-Fi down: Wi-Fi loss flag = 1");
    stale = g_cmds_stale; cmd(DL_CMD_STOP);
    CHECK(comms_get_command() == COMMS_CMD_NONE && g_cmds_stale == stale + 1, "command while Wi-Fi is down ignored");
    CHECK(s_state == COM_DISCONNECTED, "never faults: link loss only changes COM state and the status bit");

    printf("%d checks, %s (%d failed)\n", checks, fails ? "FAILED" : "ALL PASSED", fails);
    return fails != 0;
}