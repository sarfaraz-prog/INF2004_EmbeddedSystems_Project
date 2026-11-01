/*
 * buddy1_bench.c  --  Buddy 1 (WiFi / communication / telemetry) bench test.
 *
 * Runs on the Pico W, no network needed. It pretends the link goes up and
 * down and checks, on the real board, that:
 *   - the COM state changes as expected (FR-COM-03, 04, 01)
 *   - the on-board LED is solid ON in COM_SYNC / COM_CONNECTED and blinks at
 *     2 Hz in COM_DISCONNECTED (FR-COM-05, 06)
 *   - the telemetry frame is packed correctly on the target (FR-COM-07)
 *
 * Fake link timeline, repeating every 16 s:
 *    0 -  6 s  link down                      -> COM_DISCONNECTED (blink)
 *    6 - 10 s  link up, messages waiting      -> COM_SYNC         (solid)
 *   10 - 16 s  link up, nothing waiting       -> COM_CONNECTED    (solid)
 *
 * The on-board LED sits on the WiFi chip, so it is switched with
 * cyw43_arch_gpio_put(), not with a normal GPIO call.
 */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "com_state.h"
#include "status_led.h"
#include "telemetry.h"

#define LOOP_MS      10u
#define CYCLE_MS     16000u
#define DOWN_END_MS   6000u
#define SYNC_END_MS  10000u
#define REPORT_MS     1000u

static const char *state_name(com_state_t s)
{
    switch (s) {
    case COM_DISCONNECTED: return "COM_DISCONNECTED";
    case COM_SYNC:         return "COM_SYNC";
    case COM_CONNECTED:    return "COM_CONNECTED";
    default:               return "UNKNOWN";
    }
}

int main(void)
{
    stdio_init_all();
    sleep_ms(2000);                       /* time to open the serial monitor */

    if (cyw43_arch_init() != 0) {
        printf("cyw43_arch_init failed\n");
        while (true) { sleep_ms(1000); }
    }

    com_init();

    com_state_t last_state = com_get_state();
    bool last_level = false;
    uint32_t rising_edges = 0;
    uint32_t next_report = REPORT_MS;
    uint32_t t0 = to_ms_since_boot(get_absolute_time());

    printf("\nBuddy 1 bench test started (fake link, 16 s cycle)\n");

    while (true) {
        uint32_t now = to_ms_since_boot(get_absolute_time()) - t0;
        uint32_t phase = now % CYCLE_MS;

        com_inputs_t in;
        in.wifi_up     = (phase >= DOWN_END_MS);
        in.mqtt_up     = (phase >= DOWN_END_MS);
        in.queue_empty = (phase >= SYNC_END_MS);
        com_state_t st = com_update(&in);

        bool level = status_led_level(st, now);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, level);
        if (level && !last_level) {
            rising_edges++;
        }
        last_level = level;

        if (st != last_state) {
            printf("[%6lu ms] state: %s -> %s\n", (unsigned long)now,
                   state_name(last_state), state_name(st));
            last_state = st;
        }

        if (now >= next_report) {
            telemetry_sample_t s;
            uint8_t frame[DL_FRAME_BYTES];
            int n;

            memset(&s, 0, sizeof s);
            s.state = DL_STATE_CALIBRATE;
            s.speed = 25;
            s.dist = (uint8_t)((now / 100u) % 63u);
            s.dir = 90;
            s.tilt = 0;
            s.roll = 0;

            n = telemetry_pack(&s, frame, sizeof frame);
            printf("[%6lu ms] LED=%d blinks_so_far=%lu pack=%d frame=",
                   (unsigned long)now, (int)level, (unsigned long)rising_edges, n);
            for (unsigned i = 0; (n == TELEM_OK) && (i < DL_FRAME_BYTES); i++) {
                printf("%02X", (unsigned)frame[i]);
            }
            printf("\n");
            next_report += REPORT_MS;
        }

        sleep_ms(LOOP_MS);
    }
}