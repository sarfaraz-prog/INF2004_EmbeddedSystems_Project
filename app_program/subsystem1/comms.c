/*
 * comms.c  --  Buddy 1 (WiFi / MQTT communication and telemetry)  --  the engine
 *
 * See comms.h for how to use it and comms_port.h for the platform part.
 * This file has no Pico SDK, no micro T-Kernel and no radio code. The same
 * file runs on the micro T-Kernel base, on the plain-SDK bench and in the PC
 * tests.
 *
 * What happens inside:
 *
 *  - comms_send() formats a sample into a small INBOX and returns at once.
 *  - comms_task() (called by the WiFi task on micro T-Kernel) moves the inbox
 *    into trans_queue, keeps MQTT connected (retry every 5 s), publishes the
 *    OLDEST queued message with QoS 1, and removes it from the queue only
 *    after the broker confirms it (FR-COM-12).
 *  - After each MQTT connect it subscribes to the command topic. A message
 *    one byte (1 START, 2 STOP, 3 RESET, see data_layout.h) is stored for
 *    comms_get_command(). Anything else is rejected and counted.
 *
 * Who touches what (this is why no mutex is needed):
 *   inbox       written by comms_send() (task A), read by comms_task()
 *   trans_queue only touched by comms_task()
 *   command ring written by comms_task() (lwIP callback), read by comms_get_command()
 *   counters    only written by one side, read by the other (word reads)
 * The inbox and the ring each have one writer and one reader.
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "comms.h"
#include "comms_port.h"

#if COMMS_HAVE_NET

#include "lwip/ip_addr.h"
#include "lwip/apps/mqtt.h"

#include "status_led.h"
#include "trans_queue.h"


#define CMD_BUF_MAX      DL_CMD_BYTES   /* a command is one byte */
#define CMD_RING_SIZE    4u      /* holds 3 unread commands (one slot stays empty) */

#define INBOX_N          8u      /* samples waiting to enter the queue */

#ifndef COMMS_VERBOSE
#define COMMS_VERBOSE 1
#endif
#if COMMS_VERBOSE
#define LOG(...) PORT_LOG(__VA_ARGS__)
#else
#define LOG(...) ((void)0)
#endif

/* ---------- state shared with the lwIP callbacks (same task as comms_task) ---------- */
static volatile int g_mqtt_up = 0;
static volatile int g_mqtt_connecting = 0;
static volatile int g_mqtt_status = -1;
static volatile int g_pub_done = 0;
static volatile int g_pub_err = 0;
static volatile uint32_t g_pub_seq = 0;
static volatile int g_sub_done = 0;
static volatile int g_sub_err = 0;

/* Command ring: written by the lwIP callback (head), read by
 * comms_get_command() (tail). One writer and one reader. */
static volatile uint8_t g_cmd_ring[CMD_RING_SIZE];
static volatile uint8_t g_cmd_head = 0;
static volatile uint8_t g_cmd_tail = 0;
static volatile uint32_t g_cmds_accepted = 0;
static volatile uint32_t g_cmds_rejected = 0;
/* Stale-command gate (written only by comms_task and the lwIP callbacks, which run
 * in the same context). Commands are accepted only after the subscribe is confirmed
 * AND COMMS_CMD_SETTLE_MS later: a retained or replayed command arrives right after
 * the subscribe and is ignored. Closed again whenever MQTT goes down. */
static volatile bool     g_cmd_gate = false;
static volatile uint32_t g_cmd_open_at = 0;
static volatile uint32_t g_cmds_stale = 0;

/* Incoming message being assembled (a message can arrive in pieces). */
static uint8_t g_in_buf[CMD_BUF_MAX];
static uint8_t g_in_len = 0;
static bool    g_in_ok = false;      /* right topic and not too long so far */

/* Inbox: written by comms_send() (head), read by comms_task() (tail). */
typedef struct {
    uint16_t len;
    uint8_t  data[TQ_MSG_MAX];
} inbox_slot_t;
static inbox_slot_t s_inbox[INBOX_N];
static volatile uint8_t  s_in_head = 0;
static volatile uint8_t  s_in_tail = 0;
static volatile uint32_t s_inbox_drops = 0;     /* written by comms_send() only */

/* ---------- module state ---------- */
static comms_config_t s_cfg;
static mqtt_client_t *s_client = NULL;           /* created on the first comms_task() */
static struct mqtt_connect_client_info_t s_ci;
static tq_msg_t s_msg;                           /* message being sent (kept off the stack) */
static volatile bool s_ready = false;
static uint32_t s_t0 = 0;
static volatile uint32_t s_acked = 0;
static volatile com_state_t s_state = COM_DISCONNECTED;
/* Snapshots for comms_get_stats(), refreshed by comms_task(). */
static volatile uint16_t s_snap_queued = 0;
static volatile bool s_wifi_up = false;           /* written by comms_task only */
static volatile uint32_t s_snap_discarded = 0;   /* queue + inbox drops */

/* ---------- helpers ---------- */
uint32_t comms_now_ms(void)
{
    return port_now_ms() - s_t0;
}

/* ---------- lwIP callbacks (flags only) ---------- */
static void conn_cb(mqtt_client_t *c, void *arg, mqtt_connection_status_t status)
{
    (void)c;
    (void)arg;
    g_mqtt_status = (int)status;
    g_mqtt_up = (status == MQTT_CONNECT_ACCEPTED);
    g_mqtt_connecting = 0;
}

static void pub_cb(void *arg, err_t err)
{
    g_pub_seq = (uint32_t)(uintptr_t)arg;
    g_pub_err = (int)err;
    g_pub_done = 1;
}

static void sub_cb(void *arg, err_t err)
{
    (void)arg;
    g_sub_err = (int)err;
    g_sub_done = 1;
}

static void cmd_push(comms_cmd_t c)
{
    uint8_t next = (uint8_t)((g_cmd_head + 1u) % CMD_RING_SIZE);
    if (next == g_cmd_tail) {            /* ring full: drop and count */
        g_cmds_rejected++;
        return;
    }
    g_cmd_ring[g_cmd_head] = (uint8_t)c;
    PORT_MB();                           /* data first, then announce it */
    g_cmd_head = next;
    g_cmds_accepted++;
}

/* Called when a message arrives: remember if it is on the command topic. */
static void inpub_cb(void *arg, const char *topic, u32_t tot_len)
{
    (void)arg;
    g_in_len = 0;
    g_in_ok = (strcmp(topic, COMMS_TOPIC_CMD) == 0) && (tot_len <= CMD_BUF_MAX);
    if (strcmp(topic, COMMS_TOPIC_CMD) == 0 && tot_len > CMD_BUF_MAX) {
        g_cmds_rejected++;               /* too long to be a command */
    }
}

/* Called with the message data (maybe in several pieces). */
static void indata_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
    (void)arg;
    if (g_in_ok) {
        if ((unsigned)g_in_len + len <= CMD_BUF_MAX) {
            memcpy(&g_in_buf[g_in_len], data, len);
            g_in_len = (uint8_t)(g_in_len + len);
        } else {
            g_in_ok = false;
            g_cmds_rejected++;
        }
    }
    if ((flags & MQTT_DATA_FLAG_LAST) && g_in_ok &&
        (!g_cmd_gate || (int32_t)(comms_now_ms() - g_cmd_open_at) < 0)) {
        g_cmds_stale++;                  /* too early after (re)subscribe: ignore */
        g_cmds_rejected++;
        g_in_ok = false;
    }
    if ((flags & MQTT_DATA_FLAG_LAST) && g_in_ok) {
        /* Exactly one byte, value 1-3; anything else is rejected. */
        uint8_t c = telemetry_cmd_decode(g_in_buf, g_in_len);
        if (c != 0u) {
            cmd_push((comms_cmd_t)c);
        } else {
            g_cmds_rejected++;
        }
        g_in_ok = false;
    }
}

/* ---------- public API ---------- */
bool comms_init(const comms_config_t *cfg)
{
    if (cfg == NULL || cfg->wifi_ssid == NULL || cfg->wifi_password == NULL ||
        cfg->broker_ip == NULL || cfg->client_id == NULL) {
        return false;
    }
    if (s_ready) {
        return true;                     /* already initialised */
    }
    s_cfg = *cfg;
    if (s_cfg.broker_port == 0) {
        s_cfg.broker_port = 1883;
    }

    if (!port_init(&s_cfg)) {
        LOG("comms: port_init failed\n");
        return false;
    }

    tq_init();
    com_init();
    s_state = COM_DISCONNECTED;
    s_in_head = 0;
    s_in_tail = 0;

    memset(&s_ci, 0, sizeof s_ci);
    s_ci.client_id = s_cfg.client_id;
    s_ci.client_user = s_cfg.username;
    s_ci.client_pass = s_cfg.password;
    s_ci.keep_alive = KEEP_ALIVE_S;

    s_t0 = port_now_ms();
    if (port_wifi_join(&s_cfg)) {
        LOG("comms: WiFi join started\n");
    }
    PORT_MB();
    s_ready = true;                      /* last: comms_task() starts working */
    return true;
}

int comms_send(const telemetry_sample_t *in)
{
    static uint8_t  seq = 0u;            /* only touched here (one writer) */
    static uint32_t last_lost = 0u;
    telemetry_sample_t s;
    uint32_t lost;
    uint8_t h;
    uint8_t next;
    int n;

    if (!s_ready) {
        return COMMS_ERR_NOT_READY;
    }
    if (!in) {
        return TELEM_ERR_ARG;
    }
    h = s_in_head;
    next = (uint8_t)((h + 1u) % INBOX_N);
    if (next == s_in_tail) {             /* inbox full: drop this sample */
        s_inbox_drops++;
        return COMMS_ERR_BUSY;
    }
    /* Comms owns three fields; the caller's values for them are ignored. */
    s = *in;
    lost = s_snap_discarded;             /* includes s_inbox_drops */
    s.drop = (lost != last_lost) ? 1u : 0u;   /* something was dropped since last sample */
    last_lost = lost;
    s.wifi_loss = s_wifi_up ? 0u : 1u;   /* Wi-Fi radio only (broker down is not Wi-Fi loss); status, never a fault */
    s.seq = (uint8_t)(seq & DL_SEQ_MAX);
    n = telemetry_pack(&s, s_inbox[h].data, TQ_MSG_MAX);
    if (n != TELEM_OK) {
        return n;                        /* value out of range: not sent */
    }
    seq++;
    s_inbox[h].len = (uint16_t)DL_FRAME_BYTES;
    PORT_MB();                           /* message first, then announce it */
    s_in_head = next;
    return COMMS_OK;
}

comms_cmd_t comms_get_command(void)
{
    comms_cmd_t c;
    if (g_cmd_tail == g_cmd_head) {
        return COMMS_CMD_NONE;
    }
    PORT_MB();
    c = (comms_cmd_t)g_cmd_ring[g_cmd_tail];
    PORT_MB();                           /* read first, then free the slot */
    g_cmd_tail = (uint8_t)((g_cmd_tail + 1u) % CMD_RING_SIZE);
    return c;
}

com_state_t comms_get_state(void)
{
    return s_state;
}

void comms_get_stats(comms_stats_t *out)
{
    if (out == NULL) {
        return;
    }
    out->sent_acked = s_acked;
    out->discarded = s_snap_discarded;
    out->queued = s_snap_queued;
    out->cmds_accepted = g_cmds_accepted;
    out->cmds_rejected = g_cmds_rejected;
}

void comms_led_task(void)
{
    static bool last_level = false;
    bool level;
    if (!s_ready) {
        return;
    }
    level = status_led_level(s_state, comms_now_ms());
    if (level != last_level) {
        port_led(level ? 1 : 0);
        last_level = level;
    }
}

/* Move samples from the inbox into the queue (the queue drops its oldest
 * message by itself when full and counts it). */
static void inbox_drain(void)
{
    while (s_in_tail != s_in_head) {
        uint8_t t = s_in_tail;
        PORT_MB();
        (void)tq_push(s_inbox[t].data, s_inbox[t].len);
        PORT_MB();
        s_in_tail = (uint8_t)((t + 1u) % INBOX_N);
    }
}

/* What comms_task() remembers between calls. Only comms_task() and its helpers touch it. */
static struct {
    int         last_link;
    const char *last_link_text;
    int         last_mqtt_status;
    bool        was_up;
    bool        subscribed;
    bool        sub_pending;
    uint32_t    next_wifi_retry;
    uint32_t    next_mqtt_try;
    uint32_t    mqtt_attempt_since;
    uint32_t    next_sub_try;
    bool        inflight;
    uint32_t    inflight_seq;
    uint32_t    inflight_since;
} ctx = { -99, NULL, -99, false, false, false, COMMS_RETRY_MS, 0, 0, 0, false, 0, 0 };

/* Cut the MQTT connection and forget everything that depended on it.
 * mqtt_disconnect() does not call any callback, so the flags are cleared here. */
static void mqtt_drop(void)
{
    port_net_lock();
    mqtt_disconnect(s_client);
    port_net_unlock();
    g_mqtt_up = 0;
    g_mqtt_connecting = 0;
    ctx.inflight = false;
}

/* The MQTT client is created on the first call (lwIP must be up). */
static bool client_ready(void)
{
    if (s_client == NULL) {
        port_net_lock();
        s_client = mqtt_client_new();
        if (s_client != NULL) {
            mqtt_set_inpub_callback(s_client, inpub_cb, indata_cb, NULL);
        }
        port_net_unlock();
        if (s_client == NULL) {
            LOG("comms: mqtt_client_new failed\n");
            return false;
        }
    }
    return true;
}

/* Wi-Fi: log changes, drop MQTT when the link goes, retry the join every 5 s. */
static void wifi_step(uint32_t now, int link, bool up)
{
    const char *txt = port_link_text();

    if (link != ctx.last_link || txt != ctx.last_link_text) {
        if (txt[0] != '\0') {          /* micro T-Kernel: the WiFi task logs it itself */
            LOG("[%u ms] WiFi link: %s\n", (unsigned)now, txt);
        }
        ctx.last_link = link;
        ctx.last_link_text = txt;
    }
    if (up && !ctx.was_up) {
        LOG("[%u ms] WiFi UP\n", (unsigned)now);
        ctx.next_mqtt_try = now;
    }
    if (!up && ctx.was_up) {
        LOG("[%u ms] WiFi LOST\n", (unsigned)now);
        ctx.next_wifi_retry = now;
        mqtt_drop();
    }
    ctx.was_up = up;

    if (link == PORT_LINK_DOWN && now >= ctx.next_wifi_retry) {
        if (port_wifi_join(&s_cfg)) {
            LOG("[%u ms] retrying WiFi...\n", (unsigned)now);
        }
        ctx.next_wifi_retry = now + COMMS_RETRY_MS;
    }
}

/* MQTT connect / retry (every 5 s, FR-COM-02). */
static void mqtt_connect_step(uint32_t now, bool up)
{
    if (up && !g_mqtt_up && !g_mqtt_connecting && now >= ctx.next_mqtt_try) {
        ip_addr_t broker;
        if (!ipaddr_aton(s_cfg.broker_ip, &broker)) {
            LOG("[%u ms] broker_ip is not a valid address\n", (unsigned)now);
        } else {
            port_net_lock();
            err_t e = mqtt_client_connect(s_client, &broker, s_cfg.broker_port,
                                          conn_cb, NULL, &s_ci);
            port_net_unlock();
            LOG("[%u ms] MQTT connect started (err=%d)\n", (unsigned)now, (int)e);
            if (e == ERR_OK) {
                g_mqtt_connecting = 1;
                ctx.mqtt_attempt_since = now;
            }
        }
        ctx.next_mqtt_try = now + COMMS_RETRY_MS;
    }
    /* An attempt not finished after 5 s is cancelled so the next one can
     * start (lwIP would otherwise wait about 18 s). */
    if (g_mqtt_connecting && (now - ctx.mqtt_attempt_since) > COMMS_RETRY_MS) {
        mqtt_drop();
        ctx.next_mqtt_try = now;
        LOG("[%u ms] MQTT attempt timed out, retrying\n", (unsigned)now);
    }
    if (g_mqtt_status != ctx.last_mqtt_status) {
        LOG("[%u ms] MQTT status: %d (0 = connected)\n", (unsigned)now, g_mqtt_status);
        ctx.last_mqtt_status = g_mqtt_status;
    }
}

/* Subscribe to the command topic after every connect; open the command gate later. */
static void subscribe_step(uint32_t now)
{
    if (!g_mqtt_up) {
        g_cmd_gate = false;              /* no commands until subscribed again + settle time */
        ctx.subscribed = false;
        ctx.sub_pending = false;
        ctx.next_sub_try = 0;              /* subscribe at once after the next connect */
    }
    if (g_sub_done) {
        g_sub_done = 0;
        ctx.sub_pending = false;
        if (g_sub_err == ERR_OK) {
            ctx.subscribed = true;
            g_cmd_open_at = now + COMMS_CMD_SETTLE_MS;
            PORT_MB();
            g_cmd_gate = true;
            LOG("[%u ms] subscribed to %s\n", (unsigned)now, COMMS_TOPIC_CMD);
        }
    }
    if (g_mqtt_up && !ctx.subscribed && !ctx.sub_pending && now >= ctx.next_sub_try) {
        port_net_lock();
        err_t e = mqtt_subscribe(s_client, COMMS_TOPIC_CMD, 1, sub_cb, NULL);
        port_net_unlock();
        if (e == ERR_OK) {
            ctx.sub_pending = true;
        }
        ctx.next_sub_try = now + SUBSCRIBE_RETRY_MS;
    }
}

/* Publish the oldest queued message, one at a time; remove it only after the broker acks. */
static void publish_step(uint32_t now)
{
    if (g_pub_done) {
        g_pub_done = 0;
        /* Only the answer for the message we are waiting for counts. */
        if (ctx.inflight && g_pub_seq == ctx.inflight_seq) {
            ctx.inflight = false;
            if (g_pub_err == ERR_OK) {
                (void)tq_ack_oldest(ctx.inflight_seq);   /* broker confirmed */
                s_acked++;
            }
        }
    }
    if (ctx.inflight && (now - ctx.inflight_since) > COMMS_ACK_TIMEOUT_MS) {
        /* No answer to a QoS 1 publish in time: treat the link as lost (FR-COM-01).
         * The queue keeps the message; it is sent again after the reconnect. */
        LOG("[%u ms] no ack in %u ms: link lost, reconnecting\n",
            (unsigned)now, (unsigned)COMMS_ACK_TIMEOUT_MS);
        mqtt_drop();
        ctx.next_mqtt_try = now;
    }
    if (g_mqtt_up && !ctx.inflight && tq_peek_oldest(&s_msg) == TQ_OK) {
        port_net_lock();
        err_t e = mqtt_publish(s_client, COMMS_TOPIC_TELEMETRY, s_msg.data, s_msg.len,
                               1, 0, pub_cb, (void *)(uintptr_t)s_msg.seq);
        port_net_unlock();
        if (e == ERR_OK) {
            ctx.inflight = true;
            ctx.inflight_seq = s_msg.seq;
            ctx.inflight_since = now;
        }
    }
}

/* COM state and snapshots for the other tasks. */
static void state_step(bool up)
{
    com_inputs_t in;

    in.wifi_up = up;
    in.mqtt_up = (g_mqtt_up != 0);
    in.queue_empty = (tq_count() == 0u);
    s_state = com_update(&in);
    s_snap_queued = tq_count();
    s_snap_discarded = tq_discarded() + s_inbox_drops;
}

void comms_task(void)
{
    uint32_t now;
    int link;
    bool up;

    if (!s_ready) {
        return;
    }
    now = comms_now_ms();
    link = port_link();
    up = (link == PORT_LINK_UP);
    s_wifi_up = up;

    if (!client_ready()) {
        return;
    }
    inbox_drain();
    wifi_step(now, link, up);
    mqtt_connect_step(now, up);
    subscribe_step(now);
    publish_step(now);
    state_step(up);
}

#else /* !COMMS_HAVE_NET : micro T-Kernel build without NET=lwip MQTT=1 */

/* The team builds with "make" (no network) most of the time. Then comms is a
 * harmless stub: comms_send() says "not ready", there are no commands and the
 * state stays DISCONNECTED. Build with "make NET=lwip MQTT=1" for the real
 * thing. */
bool comms_init(const comms_config_t *cfg) { (void)cfg; return true; }
int  comms_send(const telemetry_sample_t *s) { (void)s; return COMMS_ERR_NOT_READY; }
void comms_task(void) {}
comms_cmd_t comms_get_command(void) { return COMMS_CMD_NONE; }
void comms_led_task(void) {}
com_state_t comms_get_state(void) { return COM_DISCONNECTED; }
void comms_get_stats(comms_stats_t *out)
{
    if (out != NULL) { memset(out, 0, sizeof *out); }
}
uint32_t comms_now_ms(void) { return port_now_ms(); }

#endif /* COMMS_HAVE_NET */