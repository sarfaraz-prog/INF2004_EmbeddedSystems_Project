# Subsystem 1: WiFi / MQTT communication and telemetry (Buddy 1)

What it does: connects the Pico W to WiFi and an MQTT broker, sends the car's
state, speed, distance, direction, hump, tilt and roll as one 9-byte binary frame once a second, keeps up to
100 messages when the link is down, and receives START / STOP / RESET commands.

Message layout (bit positions, units, ranges, invalid codes) is in
[data_layout.h](data_layout.h). Read that first. Topics: `robocar/car1/telemetry`
(car to broker, QoS 1, 9-byte binary frame) and `robocar/car1/cmd` (broker to car,
1 byte: 1 START, 2 STOP, 3 RESET).

## Files

| File                                          | What it is                                                                                        | Needs the Pico? |
|---|---|---|
| `comms.h`, `comms.c`                          | The interface for the team: `comms_init()`, `comms_send()`, `comms_task()`, `comms_get_command()` | Needs the port file above and lwIP MQTT |
| `comms_port.h`                                | The list of platform functions that `comms.c` calls (WiFi, clock, LED, lock)                      | No |
| `testing/subsystem1/bench/comms_port_sdk.c`   | The Pico SDK version of those functions. CMake must define `COMMS_PORT_SDK`                       | Yes (Pico SDK, cyw43) |
| `trans_queue.c/.h`                            | 100-message send queue, drops the oldest when full, counts drops (FR-COM-09 to 12)                | No, plain C |
| `com_state.c/.h`                              | COM_DISCONNECTED / COM_SYNC / COM_CONNECTED (FR-COM-01, 03, 04)                                   | No, plain C |
| `status_led.c/.h`                             | LED on or off for a state and time (FR-COM-05, 06)                                                | No, plain C |
| `data_layout.h`                               | The one shared table: field positions, widths, units, invalid codes, command codes                | No, constants only |
| `telemetry.c/.h`                              | Packs / unpacks the 9-byte frame and decodes the 1-byte command (FR-COM-07, NFR-RES-03)           | No, plain C |

## How to use it from another task

```c
comms_init(&cfg);                    // once, at start-up (does not block)
...
telemetry_sample_t s = { ... };      // fill from Odometry, IMU, Nav, Safety
comms_send(&s);                      // once a second
comms_task();                        // every ~10 ms
comms_led_task();                    // every ~10 ms
comms_cmd_t c = comms_get_command(); // START / STOP / RESET or COMMS_CMD_NONE
```

Call all of these from one task. `comms_task()` never blocks. This code never
touches the motors: Nav reads the command and decides what to do.

## How to test it

1. **On a PC (no Pico needed):** see `testing/subsystem1/README.md`.
2. **On the Pico W:** see `testing/subsystem1/bench/README.md`.

Results are in `testing/subsystem1/TEST_LOG.md`.

## Known open items

- No password or TLS yet (NFR-SEC-01/02 not met).
- The frame carries no battery voltage, distance or timestamp (Minimum-Bit change). A 4-bit sequence number and a drop flag replace the timestamp.
- Link-loss time (about 4 s worst case, shown on the PC clock) is not measured
  on the car with a real hotspot-off test yet. The `comms_task()` running time
  was measured on the bench: about 1.1 ms normally, 15.2 ms once during a
  connect attempt with the broker off (see `testing/subsystem1/TEST_LOG.md`).
- A command sent in the first 300 ms after the car subscribes is ignored on purpose
  (stale retained START). Resend it.
- Hump jolt unit and range are placeholders until the ramp is measured.