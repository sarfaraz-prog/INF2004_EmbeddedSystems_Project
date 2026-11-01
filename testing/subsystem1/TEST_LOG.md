# Subsystem 1 - test log

These tests check the plain-C logic against the requirements in the design review document. Tests marked auto run on a PC. Tests marked bench run on a Pico W with real WiFi and a Mosquitto broker.

## Test cases

Test IDs never change and are never reused.

| ID | What is tested | Requirement | Method | Where |
|---|---|---|---|---|
| S1-U01 | queue starts empty, discard counter 0 | FR-COM-09, FR-COM-11 | auto | `testing/subsystem1/test_trans_queue.c` |
| S1-U02 | peek and ack on an empty queue are refused | robustness | auto | `testing/subsystem1/test_trans_queue.c` |
| S1-U03 | 100 messages stored, none dropped (FR-COM-09) | FR-COM-09 | auto | `testing/subsystem1/test_trans_queue.c` |
| S1-U04 | 101st message drops the oldest and counts it (FR-COM-10, FR-COM-11) | FR-COM-10, FR-COM-11 | auto | `testing/subsystem1/test_trans_queue.c` |
| S1-U05 | oldest message is now number 2 | FR-COM-10 | auto | `testing/subsystem1/test_trans_queue.c` |
| S1-U06 | discard counter keeps counting | FR-COM-11 | auto | `testing/subsystem1/test_trans_queue.c` |
| S1-U07 | NULL pointer and zero length are rejected | robustness | auto | `testing/subsystem1/test_trans_queue.c` |
| S1-U08 | 10-byte message rejected (the frame is 9 bytes; was 257 bytes with the old text message) | NFR-RES-03 | auto | `testing/subsystem1/test_trans_queue.c` |
| S1-U09 | exactly 9 bytes accepted (was 256 with the old text message) | NFR-RES-03 | auto | `testing/subsystem1/test_trans_queue.c` |
| S1-U10 | ack of a message that was already dropped is refused | FR-COM-12 | auto | `testing/subsystem1/test_trans_queue.c` |
| S1-U11 | a refused ack does not reset the discard counter | FR-COM-12 | auto | `testing/subsystem1/test_trans_queue.c` |
| S1-U12 | 100 messages sent oldest first, in order | FR-COM-03, FR-COM-04 | auto | `testing/subsystem1/test_trans_queue.c` |
| S1-U13 | discard counter kept until the last message is acked | FR-COM-12 | auto | `testing/subsystem1/test_trans_queue.c` |
| S1-U14 | counter reset only after the queue is fully sent (FR-COM-12) | FR-COM-12 | auto | `testing/subsystem1/test_trans_queue.c` |
| S1-U15 | order is kept when the queue wraps around | FR-COM-09, FR-COM-10 | auto | `testing/subsystem1/test_trans_queue.c` |
| S1-U20 | starts in COM_DISCONNECTED at power-up | Section 7 assumption (a) | auto | `testing/subsystem1/test_com_state.c` |
| S1-U21 | WiFi up but MQTT down: stays COM_DISCONNECTED | FR-COM-03 | auto | `testing/subsystem1/test_com_state.c` |
| S1-U22 | MQTT up but WiFi down: stays COM_DISCONNECTED | FR-COM-03 | auto | `testing/subsystem1/test_com_state.c` |
| S1-U23 | both up, queue not empty: COM_SYNC (FR-COM-03) | FR-COM-03 | auto | `testing/subsystem1/test_com_state.c` |
| S1-U24 | stays in COM_SYNC while queue has messages | FR-COM-03, FR-COM-04 | auto | `testing/subsystem1/test_com_state.c` |
| S1-U25 | queue becomes empty: COM_CONNECTED (FR-COM-04) | FR-COM-04 | auto | `testing/subsystem1/test_com_state.c` |
| S1-U26 | stays in COM_CONNECTED | FR-COM-04 | auto | `testing/subsystem1/test_com_state.c` |
| S1-U27 | WiFi lost in COM_CONNECTED: COM_DISCONNECTED (FR-COM-01) | FR-COM-01 | auto | `testing/subsystem1/test_com_state.c` |
| S1-U28 | both up, queue empty from DISCONNECTED: COM_CONNECTED (FR-COM-03) | FR-COM-03 | auto | `testing/subsystem1/test_com_state.c` |
| S1-U29 | MQTT lost in COM_CONNECTED: COM_DISCONNECTED (FR-COM-01) | FR-COM-01 | auto | `testing/subsystem1/test_com_state.c` |
| S1-U30 | WiFi lost in COM_SYNC: COM_DISCONNECTED (FR-COM-01) | FR-COM-01 | auto | `testing/subsystem1/test_com_state.c` |
| S1-U31 | MQTT lost in COM_SYNC: COM_DISCONNECTED (FR-COM-01) | FR-COM-01 | auto | `testing/subsystem1/test_com_state.c` |
| S1-U32 | reconnects with empty queue: COM_CONNECTED | FR-COM-03 | auto | `testing/subsystem1/test_com_state.c` |
| S1-U33 | NULL inputs change nothing | robustness | auto | `testing/subsystem1/test_com_state.c` |
| S1-U34 | com_get_state() matches the last update | robustness | auto | `testing/subsystem1/test_com_state.c` |
| S1-U40 | solid ON in COM_CONNECTED and COM_SYNC for 10 s (FR-COM-05) | FR-COM-05 | auto | `testing/subsystem1/test_status_led.c` |
| S1-U41 | COM_DISCONNECTED: ON for the first 250 ms | FR-COM-06 | auto | `testing/subsystem1/test_status_led.c` |
| S1-U42 | COM_DISCONNECTED: OFF for the next 250 ms | FR-COM-06 | auto | `testing/subsystem1/test_status_led.c` |
| S1-U43 | pattern repeats every 500 ms | FR-COM-06 | auto | `testing/subsystem1/test_status_led.c` |
| S1-U44 | 20 blinks (40 changes) in 10 s = 2 Hz (FR-COM-06) | FR-COM-06 | auto | `testing/subsystem1/test_status_led.c` |
| S1-U45 | 50% duty cycle over 10 s | FR-COM-06 | auto | `testing/subsystem1/test_status_led.c` |
| S1-U46 | unknown state: LED off | robustness | auto | `testing/subsystem1/test_status_led.c` |
| S1-U50 | normal sample gives the expected message (FR-COM-07/09/13) **(RETIRED 2026-11-01: the text message was replaced by the binary frame; test deleted)** | FR-COM-07, FR-COM-09, FR-COM-13 | auto | `testing/subsystem1/test_telemetry.c` |
| S1-U51 | message is at most 256 bytes (NFR-RES-03) **(RETIRED 2026-11-01: the text message was replaced by the binary frame; test deleted)** | NFR-RES-03 | auto | `testing/subsystem1/test_telemetry.c` |
| S1-U52 | worst-case values still fit in 256 bytes **(RETIRED 2026-11-01: the text message was replaced by the binary frame; test deleted)** | NFR-RES-03 | auto | `testing/subsystem1/test_telemetry.c` |
| S1-U53 | negative values are written correctly **(RETIRED 2026-11-01: the text message was replaced by the binary frame; test deleted)** | FR-COM-07 | auto | `testing/subsystem1/test_telemetry.c` |
| S1-U54 | all-zero sample works **(RETIRED 2026-11-01: the text message was replaced by the binary frame; test deleted)** | FR-COM-07 | auto | `testing/subsystem1/test_telemetry.c` |
| S1-U55 | buffer with room for the end marker works **(RETIRED 2026-11-01: the text message was replaced by the binary frame; test deleted)** | NFR-RES-03 | auto | `testing/subsystem1/test_telemetry.c` |
| S1-U56 | buffer one byte too small is refused **(RETIRED 2026-11-01: the text message was replaced by the binary frame; test deleted)** | NFR-RES-03 | auto | `testing/subsystem1/test_telemetry.c` |
| S1-U57 | small buffer: nothing written past its end **(RETIRED 2026-11-01: the text message was replaced by the binary frame; test deleted)** | NFR-RES-03 | auto | `testing/subsystem1/test_telemetry.c` |
| S1-U58 | NULL pointers and size 0 are rejected **(RETIRED 2026-11-01: the text message was replaced by the binary frame; test deleted)** | robustness | auto | `testing/subsystem1/test_telemetry.c` |
| S1-U59 | state name with a quote is rejected **(RETIRED 2026-11-01: the text message was replaced by the binary frame; test deleted)** | robustness | auto | `testing/subsystem1/test_telemetry.c` |
| S1-U60 | empty state name is rejected **(RETIRED 2026-11-01: the text message was replaced by the binary frame; test deleted)** | robustness | auto | `testing/subsystem1/test_telemetry.c` |
| S1-U61 | state name without an end marker is rejected **(RETIRED 2026-11-01: the text message was replaced by the binary frame; test deleted)** | robustness | auto | `testing/subsystem1/test_telemetry.c` |
| S1-S01 | power-up with no link: COM_DISCONNECTED | FR-COM-03 | auto | `testing/subsystem1/test_scenario.c` |
| S1-S02 | COM_DISCONNECTED: LED is blinking | FR-COM-06 | auto | `testing/subsystem1/test_scenario.c` |
| S1-S03 | 3 samples while offline: stored, none sent | FR-COM-09 | auto | `testing/subsystem1/test_scenario.c` |
| S1-S04 | link back with backlog: COM_SYNC | FR-COM-03 | auto | `testing/subsystem1/test_scenario.c` |
| S1-S05 | COM_SYNC: LED stays on | FR-COM-05 | auto | `testing/subsystem1/test_scenario.c` |
| S1-S06 | backlog drained: COM_CONNECTED, queue empty | FR-COM-04 | auto | `testing/subsystem1/test_scenario.c` |
| S1-S07 | broker receives samples 1, 2, 3 in order | FR-COM-03, FR-COM-04 | auto | `testing/subsystem1/test_scenario.c` |
| S1-S08 | link lost: back to COM_DISCONNECTED | FR-COM-01 | auto | `testing/subsystem1/test_scenario.c` |
| S1-S09 | 105 offline samples: 100 kept, 5 discarded | FR-COM-09, FR-COM-10, FR-COM-11 | auto | `testing/subsystem1/test_scenario.c` |
| S1-S10 | reconnect after overflow: COM_SYNC | FR-COM-03 | auto | `testing/subsystem1/test_scenario.c` |
| S1-S11 | overflow backlog drained: COM_CONNECTED | FR-COM-04 | auto | `testing/subsystem1/test_scenario.c` |
| S1-S12 | broker gets the newest 100, oldest 5 dropped | FR-COM-10 | auto | `testing/subsystem1/test_scenario.c` |
| S1-S13 | broker order is increasing | FR-COM-04 | auto | `testing/subsystem1/test_scenario.c` |
| S1-S14 | discard counter reset after full transmission | FR-COM-12 | auto | `testing/subsystem1/test_scenario.c` |
| S1-H01 | Pico W joins WiFi and gets an IP | FR-COM-03 | manual (Pico W) | `testing/subsystem1/bench/buddy1_wifi.c` |
| S1-H02 | MQTT connects and QoS1 publish is acked by the broker | FR-COM-03, FR-COM-04 | manual (Pico W) | `testing/subsystem1/bench/buddy1_mqtt.c` |
| S1-H03 | broker down: queue grows, state COM_DISCONNECTED | FR-COM-01, FR-COM-09 | manual (Pico W) | `testing/subsystem1/bench/buddy1_mqtt.c` |
| S1-H04 | broker back: queue drained in order of ack count, COM_CONNECTED, no drops | FR-COM-04, FR-COM-12 | manual (Pico W) | `testing/subsystem1/bench/buddy1_mqtt.c` |
| S1-H05 | queue overflow drops oldest and counts them | FR-COM-10, FR-COM-11 | manual (Pico W) | `testing/subsystem1/bench/buddy1_mqtt.c` |
| S1-H06 | MQTT retry every 5 s (measured 5002 ms) | FR-COM-02 | manual (Pico W) | `testing/subsystem1/bench/buddy1_mqtt.c` |
| S1-H07 | LED solid ON when connected, blinking when broker stopped (by eye) | FR-COM-05, FR-COM-06 | manual (Pico W) | `testing/subsystem1/bench/buddy1_mqtt.c` |
| S1-H08 | LED blink rate 2 Hz within 10% (59 blinks in 30 s = 1.97 Hz) | FR-COM-06 | manual (Pico W) | `testing/subsystem1/bench/buddy1_mqtt.c` |
| S1-D01 | empty at start | FR-COM-08 | auto (fakes) | `testing/subsystem1/test_comms_cmd.c` |
| S1-D02 | byte 1 = START accepted | FR-COM-08 | auto (fakes) | `testing/subsystem1/test_comms_cmd.c` |
| S1-D03 | byte 2 = STOP accepted | FR-COM-08 | auto (fakes) | `testing/subsystem1/test_comms_cmd.c` |
| S1-D04 | byte 3 = RESET accepted | FR-COM-08 | auto (fakes) | `testing/subsystem1/test_comms_cmd.c` |
| S1-D05 | each command returned once | FR-COM-08 | auto (fakes) | `testing/subsystem1/test_comms_cmd.c` |
| S1-D06 | bad bytes (0, 4, 255, 2 bytes, empty) give no command | FR-COM-08 | auto (fakes) | `testing/subsystem1/test_comms_cmd.c` |
| S1-D07 | 5 bad messages counted as rejected | FR-COM-08 | auto (fakes) | `testing/subsystem1/test_comms_cmd.c` |
| S1-D08 | other topic ignored | FR-COM-08 | auto (fakes) | `testing/subsystem1/test_comms_cmd.c` |
| S1-D09 | other topic not counted as a bad command | FR-COM-08 | auto (fakes) | `testing/subsystem1/test_comms_cmd.c` |
| S1-D10 | old text commands (START, S) rejected | Change requirement (binary commands) | auto (fakes) | `testing/subsystem1/test_comms_cmd.c` |
| S1-D11 | 2-byte message arriving in pieces rejected | FR-COM-08 | auto (fakes) | `testing/subsystem1/test_comms_cmd.c` |
| S1-D12 | ring full: 3 kept, 1 dropped and counted | FR-COM-08 | auto (fakes) | `testing/subsystem1/test_comms_cmd.c` |
| S1-D13 | kept in order | FR-COM-08 | auto (fakes) | `testing/subsystem1/test_comms_cmd.c` |
| S1-D14 | then empty | FR-COM-08 | auto (fakes) | `testing/subsystem1/test_comms_cmd.c` |
| S1-D15 | command codes in comms.h equal the wire codes in data_layout.h | Change requirement (binary commands) | auto (fakes) | `testing/subsystem1/test_comms_cmd.c` |
| S1-C01 | send before init is refused (NOT_READY) | robustness | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C02 | comms_task before init does nothing | robustness | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C03 | init succeeds | FR-COM-02 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C04 | a second init is harmless | robustness | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C05 | init starts one WiFi join | FR-COM-02 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C06 | power-up state is DISCONNECTED | Section 7 assumption (a) | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C07 | no WiFi retry before 5 s | FR-COM-02 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C08 | WiFi retry at 5 s (FR-COM-02) | FR-COM-02 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C09 | three samples accepted while offline | FR-COM-09 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C10 | queue holds 3 | FR-COM-09 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C11 | LED blinks at about 2 Hz when DISCONNECTED | FR-COM-06 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C12 | nothing published while offline | FR-COM-09 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C13 | MQTT connect starts as soon as WiFi is up | FR-COM-02 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C14 | unanswered attempt is cancelled and retried | FR-COM-02 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C15 | retry interval is 5 s (FR-COM-02) | FR-COM-02 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C16 | subscribes to the command topic after connect | FR-COM-08 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C17 | subscribes only once per connection | FR-COM-08 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C18 | first publish is the OLDEST message | FR-COM-03, FR-COM-04 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C19 | published on the telemetry topic | FR-COM-07 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C20 | message stays queued until the broker confirms | FR-COM-12 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C21 | second message sent after first ack | FR-COM-12 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C22 | third message sent, order kept | FR-COM-04 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C23 | state is SYNC while messages are waiting | FR-COM-03 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C24 | queue empty, 3 acked | FR-COM-12 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C25 | state is CONNECTED when the queue is empty | FR-COM-04 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C26 | LED solid ON when CONNECTED | FR-COM-05 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C27 | live message is published | FR-COM-07 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C28 | failed ack: message NOT removed (FR-COM-12) | FR-COM-12 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C29 | failed ack: same message sent again | FR-COM-12 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C30 | no ack within COMMS_ACK_TIMEOUT_MS (3 s): link treated as lost, DISCONNECTED (FR-COM-01) | FR-COM-01 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C31 | message kept after the link loss | FR-COM-09, FR-COM-12 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C32 | subscribes again after the reconnect | FR-COM-08 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C33 | after reconnect the same message is sent again | FR-COM-12 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C34 | message removed only after the ack | FR-COM-12 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C35 | late ack for an old message does not remove the current one | FR-COM-12 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C36 | the right ack removes it | FR-COM-12 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C37 | WiFi lost: DISCONNECTED | FR-COM-01 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C38 | messages kept while WiFi is down | FR-COM-09 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C39 | subscribes again after WiFi returns | FR-COM-08 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C40 | after reconnect the oldest queued message goes first | FR-COM-03, FR-COM-04 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C41 | then the next, order kept | FR-COM-04 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C42 | back to CONNECTED when drained | FR-COM-04 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C43 | link loss detected within 4.1 s, worst case (1 s sample gap + 3 s ack timeout) (FR-COM-01) | FR-COM-01 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C44 | queue empty before the overflow test | FR-COM-09 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C45 | 105 samples accepted one at a time | FR-COM-09 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C46 | queue holds 100, 5 oldest discarded (FR-COM-09/10/11) | FR-COM-09, FR-COM-10, FR-COM-11 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C47 | after overflow the oldest KEPT message goes first (sample 6) | FR-COM-10, FR-COM-11 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C48 | command received while sending telemetry | FR-COM-08 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C49 | init rejects a missing SSID | robustness | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C50 | send rejects a NULL sample | robustness | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C51 | inbox holds 7: the next 13 samples get BUSY | NFR-RES-03 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C52 | the 13 dropped samples are counted as discarded | FR-COM-09, FR-COM-10, FR-COM-11 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C53 | the 7 accepted samples reached the queue | FR-COM-09 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C54 | and the first one is sent first | FR-COM-04 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C55 | 500 samples at 1 per task call: none lost | FR-COM-09 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C56 | every lwIP call was made with the port lock held | NFR-MNT-02 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-C57 | lock and unlock are always paired | NFR-MNT-02 | auto (fakes) | `testing/subsystem1/test_comms_task.c` |
| S1-T01 | 200000 random samples: same text as snprintf **(RETIRED 2026-11-01: the text message was replaced by the binary frame; test deleted)** | NFR-RES-03 | auto (PC) | `testing/subsystem1/test_telemetry_nolibc.c` |
| S1-T02 | min/max values: same text as snprintf **(RETIRED 2026-11-01: the text message was replaced by the binary frame; test deleted)** | NFR-RES-03 | auto (PC) | `testing/subsystem1/test_telemetry_nolibc.c` |
| S1-T03 | all zero: same text as snprintf **(RETIRED 2026-11-01: the text message was replaced by the binary frame; test deleted)** | NFR-RES-03 | auto (PC) | `testing/subsystem1/test_telemetry_nolibc.c` |
| S1-T04 | sizes 1..120: right result, nothing written past the buffer **(RETIRED 2026-11-01: the text message was replaced by the binary frame; test deleted)** | NFR-RES-03 | auto (PC) | `testing/subsystem1/test_telemetry_nolibc.c` |
| S1-T05 | NULL sample refused **(RETIRED 2026-11-01: the text message was replaced by the binary frame; test deleted)** | robustness | auto (PC) | `testing/subsystem1/test_telemetry_nolibc.c` |
| S1-T06 | NULL buffer refused **(RETIRED 2026-11-01: the text message was replaced by the binary frame; test deleted)** | robustness | auto (PC) | `testing/subsystem1/test_telemetry_nolibc.c` |
| S1-T07 | size 0 refused **(RETIRED 2026-11-01: the text message was replaced by the binary frame; test deleted)** | robustness | auto (PC) | `testing/subsystem1/test_telemetry_nolibc.c` |
| S1-T08 | quote in state name refused **(RETIRED 2026-11-01: the text message was replaced by the binary frame; test deleted)** | robustness | auto (PC) | `testing/subsystem1/test_telemetry_nolibc.c` |
| S1-T09 | empty state name refused **(RETIRED 2026-11-01: the text message was replaced by the binary frame; test deleted)** | robustness | auto (PC) | `testing/subsystem1/test_telemetry_nolibc.c` |
| S1-T10 | state name without end marker refused **(RETIRED 2026-11-01: the text message was replaced by the binary frame; test deleted)** | robustness | auto (PC) | `testing/subsystem1/test_telemetry_nolibc.c` |
| S1-B01 | all-minimum values round trip | Minimum-Bit change | auto | `testing/subsystem1/test_telemetry.c` |
| S1-B02 | all-maximum values round trip | Minimum-Bit change | auto | `testing/subsystem1/test_telemetry.c` |
| S1-B03 | invalid codes round trip | Version C: invalid code per field | auto | `testing/subsystem1/test_telemetry.c` |
| S1-B04 | known frame A matches (made by an independent Python encoder) | Minimum-Bit change | auto | `testing/subsystem1/test_telemetry.c` |
| S1-B05 | known frame B matches (independent encoder) | Minimum-Bit change | auto | `testing/subsystem1/test_telemetry.c` |
| S1-B06 | state 8 rejected (one beyond the limit) | Version C: tests at field limits | auto | `testing/subsystem1/test_telemetry.c` |
| S1-B07 | speed 64 rejected | Version C: tests at field limits | auto | `testing/subsystem1/test_telemetry.c` |
| S1-B08 | distance 64 rejected | Version C: tests at field limits | auto | `testing/subsystem1/test_telemetry.c` |
| S1-B09 | direction 360 rejected | Version C: tests at field limits | auto | `testing/subsystem1/test_telemetry.c` |
| S1-B10 | hump height 129 rejected | Version C: tests at field limits | auto | `testing/subsystem1/test_telemetry.c` |
| S1-B11 | tilt 181 rejected | Version C: tests at field limits | auto | `testing/subsystem1/test_telemetry.c` |
| S1-B12 | roll -181 rejected | Version C: tests at field limits | auto | `testing/subsystem1/test_telemetry.c` |
| S1-B13 | sequence 16 rejected | Version C: sequence number | auto | `testing/subsystem1/test_telemetry.c` |
| S1-B14 | 1-bit flag value 2 rejected | Version C: tests at field limits | auto | `testing/subsystem1/test_telemetry.c` |
| S1-B15 | setting one field does not change its neighbours | NFR-MNT-02 (packed data) | auto | `testing/subsystem1/test_telemetry.c` |
| S1-B16 | NULL pointer or 8-byte buffer rejected | robustness | auto | `testing/subsystem1/test_telemetry.c` |
| S1-B17 | wrong version byte rejected | Minimum-Bit change (version byte) | auto | `testing/subsystem1/test_telemetry.c` |
| S1-B18 | wrong frame length rejected | robustness | auto | `testing/subsystem1/test_telemetry.c` |
| S1-B19 | command byte 2 = STOP | Change requirement (binary commands) | auto | `testing/subsystem1/test_telemetry.c` |
| S1-B20 | command byte 0 rejected | Change requirement (binary commands) | auto | `testing/subsystem1/test_telemetry.c` |
| S1-B21 | command byte 4 rejected | Change requirement (binary commands) | auto | `testing/subsystem1/test_telemetry.c` |
| S1-B22 | wrong length or NULL command rejected | robustness | auto | `testing/subsystem1/test_telemetry.c` |
| S1-L01 | layout: fields add up to 57 bits, status + sequence 7 bits, total 64, frame 9 bytes, no overlaps | Minimum-Bit change, Version C (one shared header) | auto | `testing/subsystem1/test_data_layout.c` |
| S1-F01 | send accepted | Version C: sequence + drop flag, FR-COM-10/11 | auto (fakes) | `testing/subsystem1/test_comms_frame.c` |
| S1-F02 | first frame has sequence 0 (caller's value ignored) | Version C: sequence + drop flag, FR-COM-10/11 | auto (fakes) | `testing/subsystem1/test_comms_frame.c` |
| S1-F03 | Wi-Fi loss flag set while Wi-Fi is down | Version C: sequence + drop flag, FR-COM-10/11 | auto (fakes) | `testing/subsystem1/test_comms_frame.c` |
| S1-F04 | drop flag 0 when nothing was dropped (caller's value ignored) | Version C: sequence + drop flag, FR-COM-10/11 | auto (fakes) | `testing/subsystem1/test_comms_frame.c` |
| S1-F05 | sequence counts 0-15 and wraps | Version C: sequence + drop flag, FR-COM-10/11 | auto (fakes) | `testing/subsystem1/test_comms_frame.c` |
| S1-F06 | drop flag set on the first frame after a drop | Version C: sequence + drop flag, FR-COM-10/11 | auto (fakes) | `testing/subsystem1/test_comms_frame.c` |
| S1-F07 | drop flag clears when nothing new was dropped | Version C: sequence + drop flag, FR-COM-10/11 | auto (fakes) | `testing/subsystem1/test_comms_frame.c` |
| S1-F08 | out-of-range value refused, not sent | Version C: sequence + drop flag, FR-COM-10/11 | auto (fakes) | `testing/subsystem1/test_comms_frame.c` |
| S1-F09 | refused sample did not enter the inbox | Version C: sequence + drop flag, FR-COM-10/11 | auto (fakes) | `testing/subsystem1/test_comms_frame.c` |
| S1-V01 | Wi-Fi down at start: Wi-Fi loss flag = 1 | Version B: Wi-Fi loss is a status bit | auto (fakes) | `testing/subsystem1/test_comms_fresh.c` |
| S1-V02 | Wi-Fi up, broker not reached: Wi-Fi loss flag = 0 | Version C: broker-down vs Wi-Fi-down | auto (fakes) | `testing/subsystem1/test_comms_fresh.c` |
| S1-V03 | command before the subscribe is confirmed is ignored and counted | Version C: command freshness | auto (fakes) | `testing/subsystem1/test_comms_fresh.c` |
| S1-V04 | (helper check: nothing queued yet) | - | auto (fakes) | `testing/subsystem1/test_comms_fresh.c` |
| S1-V05 | retained START right after subscribe is ignored | Version C: stale/retained START | auto (fakes) | `testing/subsystem1/test_comms_fresh.c` |
| S1-V06 | fresh command after the 300 ms settle time is accepted | FR-COM-08 | auto (fakes) | `testing/subsystem1/test_comms_fresh.c` |
| S1-V07 | broker down, Wi-Fi up: COM state DISCONNECTED but Wi-Fi loss flag = 0 | Version C: broker-down vs Wi-Fi-down | auto (fakes) | `testing/subsystem1/test_comms_fresh.c` |
| S1-V08 | command while the broker is down is ignored | Version C: command freshness | auto (fakes) | `testing/subsystem1/test_comms_fresh.c` |
| S1-V09 | stale START replayed after a reconnect is ignored | Version C: stale/retained START | auto (fakes) | `testing/subsystem1/test_comms_fresh.c` |
| S1-V10 | a new START after the settle time works | FR-COM-08 | auto (fakes) | `testing/subsystem1/test_comms_fresh.c` |
| S1-V11 | Wi-Fi down: Wi-Fi loss flag = 1 | Version B: Wi-Fi loss is a status bit | auto (fakes) | `testing/subsystem1/test_comms_fresh.c` |
| S1-V12 | command while Wi-Fi is down is ignored | Version C: command freshness | auto (fakes) | `testing/subsystem1/test_comms_fresh.c` |
| S1-V13 | link loss only changes the COM state and the status bit (never a fault) | Version B: not in FAULT_STOP | auto (fakes) | `testing/subsystem1/test_comms_fresh.c` |
| S1-R01 | 200000 samples accepted by comms_send | NFR-MNT-02, FR-COM-12 | auto (2 threads) | `testing/subsystem1/test_comms_threads.c` |
| S1-R02 | all 200000 reached the broker (none lost) | NFR-MNT-02, FR-COM-12 | auto (2 threads) | `testing/subsystem1/test_comms_threads.c` |
| S1-R03 | in strict order, no gaps (none repeated, reordered or missing) | NFR-MNT-02, FR-COM-12 | auto (2 threads) | `testing/subsystem1/test_comms_threads.c` |
| S1-R04 | every frame valid: 9 bytes, right version byte (none corrupted) | NFR-MNT-02, FR-COM-12 | auto (2 threads) | `testing/subsystem1/test_comms_threads.c` |
| S1-R05 | nothing discarded in the queue | NFR-MNT-02, FR-COM-12 | auto (2 threads) | `testing/subsystem1/test_comms_threads.c` |
| S1-R06 | 100000 commands received, in order, none lost | NFR-MNT-02, FR-COM-08 | auto (2 threads) | `testing/subsystem1/test_comms_threads.c` |
| S1-H10 | Pico W joins WiFi and connects to the broker | FR-COM-02 | bench | `testing/subsystem1/bench/buddy1_mqtt.c` |
| S1-H11 | telemetry reaches the broker (QoS 1), seen in the broker log and in `mosquitto_sub` | FR-COM-07, FR-COM-12 | bench | `testing/subsystem1/bench/buddy1_mqtt.c` |
| S1-H12 | START, STOP, RESET accepted; wrong text rejected and counted | FR-COM-08 | bench | `testing/subsystem1/bench/buddy1_mqtt.c` |
| S1-H13 | broker started late: about 100 queued messages sent after it came up | FR-COM-09, FR-COM-04 | bench | `testing/subsystem1/bench/buddy1_mqtt.c` |
| S1-H14 | false link-lost events with ack timeout 0.9 s versus 3 s (10.7 min run) | FR-COM-01 | bench | `testing/subsystem1/bench/buddy1_mqtt.c` |
| S1-H15 | comms_task run time (max and CPU load) printed on the status line | NFR-RTM-03 | bench | `testing/subsystem1/bench/buddy1_mqtt.c` |
| S1-H20 | binary frames reach the broker; decoded values equal the TX lines on the serial monitor; every frame is 9 bytes; sequence counts 0-15 | Minimum-Bit change, FR-COM-07 | bench | `testing/subsystem1/bench/buddy1_mqtt.c`, `tools/decode_frame.py` |
| S1-H21 | command bytes 1, 2, 3 (START, STOP, RESET) accepted | Change requirement (binary commands), FR-COM-08 | bench | `tools/make_cmd_files.py` |
| S1-H22 | bad command files (0, 4, 2 bytes, old text START) rejected and counted | Change requirement (binary commands) | bench | `tools/make_cmd_files.py` |
| S1-H23 | retained START is ignored after a reconnect (real broker) | Version C: stale/retained START | bench | `tools/make_cmd_files.py` |

*Method:* **auto** = a check in the test program, result printed on the console. **auto (fakes)** = PC test where the platform, WiFi and broker are replaced by fakes in `testing/subsystem1/stubs/`; it checks the logic only, not the real radio or broker. **auto (2 threads)** = two PC threads share the inbox and the command ring. **manual (Pico W)** and **bench** = run on the Pico W with real WiFi and a real Mosquitto broker. *robustness* means the test protects against bad input and does not map to one requirement.

## Results

Add a new row for every run. Never edit or delete old rows.

| Date | Test IDs | Commit | Tester | Result | Log / evidence | Notes |
|---|---|---|---|---|---|---|
| 2026-10-12 | S1-U01 to S1-U15 | `933b1c7` | Sarfaraz | PASS (15/15) | `logs/2026-10-12_unit.txt` | PC build, gcc, strict warnings |
| 2026-10-12 | S1-U20 to S1-U34 | `ab73ddc` | Sarfaraz | PASS (15/15) | `logs/2026-10-12_com_state_unit.txt` | PC build, gcc, strict warnings |
| 2026-10-12 | S1-U40 to S1-U46 | `c781c91` | Sarfaraz | PASS (7/7) | `logs/2026-10-12_status_led_unit.txt` | PC build, gcc, strict warnings |
| 2026-10-12 | S1-U50 to S1-U61 | `eda6118` | Sarfaraz | PASS (12/12) | `logs/2026-10-12_telemetry_unit.txt` | PC build, gcc, strict warnings |
| 2026-10-12 | S1-S01 to S1-S14 | `e6b75eb` | Sarfaraz | PASS (14/14) | `logs/2026-10-12_scenario.txt` | PC build, gcc, strict warnings, ASan/UBSan |
| 2026-10-15 | S1-H01 to S1-H06 | `762234d` | Sarfaraz | PASS (6/6) | `logs/2026-10-15_wifi_join.txt`, `logs/2026-10-15_mqtt_offline_reconnect.txt`, `logs/2026-10-15_mqtt_retry_5s.txt` | Pico W on phone hotspot, Mosquitto on laptop, no SSID/IP in logs |
| 2026-10-16 | S1-D01 to S1-D15 | `b999c89` | Sarfaraz | PASS (15/15) | `logs/2026-10-16_pc_tests.txt` | PC, gcc, ASan/UBSan, fakes |
| 2026-10-16 | S1-C01 to S1-C57 | `b999c89` | Sarfaraz | PASS (57/57) | `logs/2026-10-16_pc_tests.txt` | PC, fakes; S1-C43 worst case link loss 4010 ms on the fake clock, ack timeout 3000 ms |
| 2026-10-16 | S1-T01 to S1-T10 | `b999c89` | Sarfaraz | PASS (10/10) | `logs/2026-10-16_pc_tests.txt` | PC; new message writer gives the same text as snprintf on 200000 random samples |
| 2026-10-16 | S1-R01 to S1-R06 | `b999c89` | Sarfaraz | PASS (6/6) | `logs/2026-10-16_pc_tests.txt` | PC, 2 threads, 200000 samples + 100000 commands; x86 only, does not prove the memory barriers |
| 2026-10-16 | S1-H10 to S1-H13 | `b999c89` | Sarfaraz | PASS | `logs/2026-10-16_bench_sub.txt` (broker side: messages received). Commands and queue backlog were seen on the serial monitor, not saved as a file | one run per condition; plain TCP, anonymous broker, `bat` is a fake 7420 |
| 2026-10-16 | S1-H14 | `b999c89` | Sarfaraz | PASS with limit | serial monitor output, not saved as a file | 0.9 s: false link-lost about every 40 s. 3 s: 1 event in 10.7 min (about 16 expected). One run only. Serial capture started at 39 s |
| 2026-10-16 | S1-H15 | `b999c89` | Sarfaraz | PASS | serial monitor output, not saved as a file | normal at most about 1.1 ms; worst 15.2 ms seen once (connect attempt with broker off) |
| 2026-11-01 | S1-U01 to S1-U15, S1-U20 to S1-U46, S1-S01 to S1-S14, S1-C01 to S1-C57, S1-D01 to S1-D15, S1-R01 to S1-R06, S1-B01 to S1-B22, S1-L01, S1-F01 to S1-F09, S1-V01 to S1-V13 | `(fill in after commit)` | Sarfaraz | PASS (all 11 programs) | `logs/2026-11-01_pc_tests.txt` (save the output of `sh testing/subsystem1/run_pc_tests.sh`) | PC, gcc, ASan/UBSan, fakes. Binary frame and 1-byte commands. S1-D rewritten for binary commands, S1-U08/U09 changed to the 9-byte frame. S1-U50 to S1-U61 and S1-T01 to S1-T10 retired. x86 only, does not prove the memory barriers |

## Not yet tested
- Link-loss fix (3 s ack timeout): tested in one 10.7 minute run only. Repeat it once more.
- NFR-PRF-05 (99% delivery, latency under 500 ms) and NFR-REL-03 (reconnect within 15 s in 95% of trials): need repeated runs.
- NFR-SEC-01 and NFR-SEC-02 (authentication, TLS): not implemented. The bench broker is anonymous and plain TCP.
- NFR-STD-02 (keep-alive 1 s): the code uses 10 s plus a 3 s ack timeout. The requirement text must be changed.
- FR-COM-07 / FR-COM-13: the binary frame no longer carries battery voltage, distance or a timestamp (Minimum-Bit change, Version B). The low-battery status bit is set by the caller.
- QoS 1 can deliver a duplicate after a reconnect. The frame has no timestamp now, so the receiver should ignore a repeated sequence number (4 bits, wraps every 16).
- Memory-barrier rules (write data before the index; read data before freeing the slot): checked by reading the code only. The PC thread test cannot prove them.
- Binary version on the Pico W (S1-H20 to S1-H23): not run yet. Until then the binary frame and 1-byte commands are tested on a PC with fakes only.
- The 300 ms command settle time after a subscribe (COMMS_CMD_SETTLE_MS) means a real command sent in that window is ignored. Not measured on the bench.
- Barr C coding standard (NFR-STD-01): not checked against the standard document.
