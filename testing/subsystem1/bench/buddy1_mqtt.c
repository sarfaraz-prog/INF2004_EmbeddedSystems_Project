/*
 * buddy1_mqtt.c  --  Buddy 1 bench demo for the comms component.
 *
 * This file shows how the rest of the team uses comms.h. It only calls the
 * public API:
 *     comms_init()         start WiFi + MQTT
 *     comms_send()         queue one telemetry sample (once a second)
 *     comms_task()         reconnect / send queued messages / get commands
 *     comms_led_task()     status LED
 *     comms_get_command()  START / STOP / RESET from the broker
 *
 * It also measures how long comms_task() takes each time it is called
 * (the "comms_task: max ... CPU load" line printed each second). This is the
 * running-time (WCET) and CPU-load evidence the design review asked for.
 * The numbers are an upper bound: background WiFi work can interrupt the
 * measurement.
 *
 * The sample values below are FAKE and are sent as the 9-byte binary frame (data_layout.h). In the real car they come from the other
 * buddies (Odometry, IMU, Nav, Safety). See COMMS_FORMAT.md.
 *
 * Needs bench/wifi_secrets.h (copy wifi_secrets.example.h and fill it in).
 * It is private and is not committed.
 */
#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"

#include "comms.h"
#include "wifi_secrets.h"

#define LOOP_MS    10u
#define SAMPLE_MS  1000u

static const char *state_name(com_state_t s)
{
    switch (s) {
    case COM_DISCONNECTED: return "DISCONNECTED";
    case COM_SYNC:         return "SYNC";
    case COM_CONNECTED:    return "CONNECTED";
    default:               return "UNKNOWN";
    }
}

static const char *cmd_name(comms_cmd_t c)
{
    switch (c) {
    case COMMS_CMD_START: return "START";
    case COMMS_CMD_STOP:  return "STOP";
    case COMMS_CMD_RESET: return "RESET";
    default:              return "NONE";
    }
}

/* Fake data standing in for the other buddies. Values move every second so the
 * laptop decoder shows them changing. Units and ranges: data_layout.h. */
static void fill_fake_sample(telemetry_sample_t *s, bool running, uint32_t n)
{
    memset(s, 0, sizeof *s);
    s->state  = running ? DL_STATE_LINE_FOLLOWING : DL_STATE_CALIBRATE;
    s->speed  = running ? (uint8_t)(20u + n % 10u) : 0u;        /* cm/s */
    s->dist   = (uint8_t)(10u + (n * 3u) % 50u);                /* cm */
    s->dir    = (uint16_t)((n * 15u) % 360u);                   /* degrees */
    s->hump_h = DL_HUMP_H_INVALID;                              /* no reading */
    s->tilt   = (int16_t)(((int32_t)(n % 7u) - 3) * 5);         /* -15..15 degrees */
    s->roll   = TELEM_ANGLE_INVALID;                            /* no reading */
    s->line   = (uint8_t)(n % 4u);
}

int main(void)
{
    stdio_init_all();
    sleep_ms(2000);

    comms_config_t cfg = {
        .wifi_ssid = WIFI_SSID,
        .wifi_password = WIFI_PASSWORD,
        .broker_ip = MQTT_BROKER_IP,
        .broker_port = MQTT_BROKER_PORT,
        .client_id = MQTT_CLIENT_ID,
        .username = NULL,          /* anonymous: bench only */
        .password = NULL,
    };

    printf("\nBuddy 1 comms demo. Topics: %s (out), %s (in)\n",
           COMMS_TOPIC_TELEMETRY, COMMS_TOPIC_CMD);
    if (!comms_init(&cfg)) {
        printf("comms_init failed\n");
        while (true) { sleep_ms(1000); }
    }

    bool running = true;               /* stands in for Nav's state */
    uint32_t next_sample = SAMPLE_MS;
    uint32_t next_report = SAMPLE_MS;
    uint32_t n_sample = 0;            /* sample number, for the fake values */
    uint32_t win_max_us = 0;          /* longest comms_task() call this second */
    uint32_t all_max_us = 0;          /* longest since power-up */
    uint64_t win_total_us = 0;        /* time spent in comms_task() this second */
    uint64_t win_start_us = time_us_64();

    while (true) {
        uint32_t now = comms_now_ms();

        /* 1 Hz: queue a sample (the car's code would do this) */
        if (now >= next_sample) {
            telemetry_sample_t s;
            fill_fake_sample(&s, running, n_sample);
            int r = comms_send(&s);
            /* what we meant to send: compare with the laptop decoder output */
            printf("[%6lu ms] TX state=%u speed=%u dist=%u dir=%u tilt=%d line=%u  (send=%d)\n",
                   (unsigned long)now, (unsigned)s.state, (unsigned)s.speed, (unsigned)s.dist,
                   (unsigned)s.dir, (int)s.tilt, (unsigned)s.line, r);
            n_sample++;
            next_sample += SAMPLE_MS;
        }

        /* the two calls that keep everything alive */
        uint64_t t_a = time_us_64();
        comms_task();
        uint32_t d_us = (uint32_t)(time_us_64() - t_a);
        win_total_us += d_us;
        if (d_us > win_max_us) { win_max_us = d_us; }
        if (d_us > all_max_us) { all_max_us = d_us; }
        comms_led_task();

        /* commands from the broker (Nav would read these) */
        comms_cmd_t c = comms_get_command();
        if (c != COMMS_CMD_NONE) {
            printf("[%6lu ms] COMMAND RECEIVED: %s\n", (unsigned long)now, cmd_name(c));
            if (c == COMMS_CMD_START) { running = true; }
            if (c == COMMS_CMD_STOP)  { running = false; }
        }

        /* once-a-second status line */
        if (now >= next_report) {
            comms_stats_t st;
            comms_get_stats(&st);
            printf("[%6lu ms] state=%s queue=%u acked=%lu dropped=%lu cmds_ok=%lu cmds_bad=%lu\n",
                   (unsigned long)now, state_name(comms_get_state()),
                   (unsigned)st.queued, (unsigned long)st.sent_acked,
                   (unsigned long)st.discarded, (unsigned long)st.cmds_accepted,
                   (unsigned long)st.cmds_rejected);
            uint64_t win_us = time_us_64() - win_start_us;
            printf("            comms_task: max %lu us (since boot %lu us), CPU load %lu.%lu %%\n",
                   (unsigned long)win_max_us, (unsigned long)all_max_us,
                   (unsigned long)((win_total_us * 100u) / win_us),
                   (unsigned long)(((win_total_us * 1000u) / win_us) % 10u));
            win_max_us = 0;
            win_total_us = 0;
            win_start_us = time_us_64();
            next_report += SAMPLE_MS;
        }

        sleep_ms(LOOP_MS);
    }
}