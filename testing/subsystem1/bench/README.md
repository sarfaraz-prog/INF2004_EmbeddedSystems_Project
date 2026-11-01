# Bench test for Subsystem 1 (Pico W + WiFi + MQTT, plain Pico SDK)

This tests the comms component (`comms.c`) on a real Pico W, using the plain Pico SDK
and a real Mosquitto broker. The PC tests in the folder above use fakes instead.

You need: a Pico W, a USB cable, a WiFi network (2.4 GHz, a phone hotspot works), and a
computer on the **same** network running the Mosquitto broker. No other wiring is needed.
The onboard LED sits on the WiFi chip.

## What connects to what

    Pico W  --WiFi-->  router / phone hotspot  <--WiFi--  laptop (Mosquitto on port 1883)
    Pico W  --USB-->   laptop (serial monitor, also flashes the Pico)

## Set up (once)

1. Copy `wifi_secrets.example.h` to `wifi_secrets.h` in this folder. Fill in
   `WIFI_SSID`, `WIFI_PASSWORD`, `MQTT_BROKER_IP` (the laptop's IP on that
   WiFi: run `ipconfig` and read the Wi-Fi IPv4 address) and `MQTT_CLIENT_ID`.
   `wifi_secrets.h` is private. It is ignored by git. Never commit it.
2. Install Mosquitto on the laptop. Config file: `listener 1883` and
   `allow_anonymous true` (bench only, no password, no TLS). Allow it through the
   Windows firewall on **Private** networks, and set the WiFi network type to Private.
3. Start the broker: `mosquitto -c mosquitto.conf -v`.

## Build and flash

The bench needs two small changes in the root `CMakeLists.txt` (the file list and one
definition), because `comms.c` has a separate platform part:

    add_executable(buddy1_mqtt
      testing/subsystem1/bench/buddy1_mqtt.c testing/subsystem1/bench/comms_port_sdk.c
      ${S1_DIR}/comms.c ${S1_DIR}/trans_queue.c ${S1_DIR}/com_state.c
      ${S1_DIR}/status_led.c ${S1_DIR}/telemetry.c)
    target_compile_definitions(buddy1_mqtt PRIVATE COMMS_PORT_SDK)

Keep your own paths for the first line. Only `comms_port_sdk.c` and the
`target_compile_definitions` line are new. From the repo root (Ubuntu / WSL):

    cmake -S . -B ../build-robocar
    cmake --build ../build-robocar -j4

Hold BOOTSEL, plug in the Pico W, and copy `buddy1_mqtt.uf2` to the RPI-RP2
drive. Open the serial monitor at 115200.

## What you should see

- Serial: `WiFi UP`, then `MQTT status: 0`, then once a second
  `state=CONNECTED queue=... acked=...` (acked goes up).
- LED: solid ON when connected, blinking (2 Hz) when not.
- Once a second: `comms_task: max ... us ... CPU load ... %`.

## Tests to run (record each in `logs/` and `TEST_LOG.md`)

| # | Do this | Expected | Test ID |
|---|---|---|---|
| 1 | On the laptop: `pip install paho-mqtt` (once), then `python tools\decode_frame.py --live --log frames.txt` | one decoded line per second, each `9 bytes`; speed, dist, dir equal the `TX` lines on the serial monitor; `seq` counts 0-15 | S1-H10, S1-H11, S1-H20 |
| 2 | `python tools\make_cmd_files.py` (once), then `& "C:\Program Files\mosquitto\mosquitto_pub.exe" -h localhost -t robocar/car1/cmd -f start.bin` (also `stop.bin`, `reset.bin`) | serial: `COMMAND RECEIVED: START` | S1-H12, S1-H21 |
| 3 | Send `-f bad0.bin`, `-f bad4.bin`, `-f two_bytes.bin`, `-f old_text_start.bin` | `cmds_bad` goes up by 1 each, no command | S1-H22 |
| 4 | Stop the broker (Ctrl+C), wait 20 s, start it again | state DISCONNECTED, LED blinks, queue grows, then all queued messages arrive in order and `queue=0` | S1-H13 |
| 5 | Leave it running for 10 minutes | count the link-lost events on the serial monitor | S1-H14 |
| 6 | Switch the hotspot off, then on | state DISCONNECTED within about 4 s (note the real time), then it reconnects | not done yet |

Save the serial output and the `mosquitto_sub` output. Remove any SSID, IP or
password from the saved files before committing.

## Results so far (2026-10-16, one run per condition)

- Connect, QoS 1 delivery, commands accepted and rejected, and store-and-forward
  (about 100 queued messages sent after the broker started late) all worked.
- `comms_task` run time: normally at most about 1.1 ms. One 15.2 ms was seen, during a
  connect attempt with the broker off.
- The ack timeout was 0.9 s at first and gave false link-lost events. With 3 s
  (`COMMS_ACK_TIMEOUT_MS` in `comms.h`) a 10.7 minute run had 1 event, where about 16
  were expected. This is one run only. The serial capture of that run started at 39 s.

## Known limits

- The hotspot-off test (row 6) is not done, so the real detection time is unknown.
  The PC test gives 4.0 s worst case on a fake clock.
- The broker is anonymous and plain TCP (no NFR-SEC-01 or NFR-SEC-02).
- The sample values in the bench are fake. The frame has no battery voltage, distance or
  timestamp.
- QoS 1 can deliver a duplicate message after a reconnect. The receiver should ignore a repeated sequence number
  (4 bits, wraps every 16); the frame has no timestamp.