#!/usr/bin/env python3
"""Decode Buddy 1 telemetry frames (9 bytes) on the laptop.
Written separately from the C code, from the table in data_layout.h, so it also checks it.

Live:   pip install paho-mqtt     (once)
        python decode_frame.py --live [broker_host] [--log frames.txt]     (default host: localhost)
Pipe:   mosquitto_sub -h localhost -t robocar/car1/telemetry -F "%x\\n" | python decode_frame.py
        (on Windows the pipe can sit buffered for minutes: use --live instead)
Test:   python decode_frame.py --selftest
Tip:    add --log frames.txt to keep the raw hex lines as evidence.
"""
import sys

STATES = ["CALIBRATE", "LINE_FOLLOWING", "LINE_LOST", "BARCODE_EXECUTION",
          "LINE_SEARCH", "OBSTACLE_AVOIDANCE", "BARCODE_RESCAN", "FAULT_STOP"]
# name, shift, bits
F = [("state", 0, 3), ("error", 3, 1), ("wifi_loss", 4, 1), ("low_batt", 5, 1), ("drop", 6, 1),
     ("hump", 7, 1), ("speed", 8, 6), ("bc_dir", 14, 2), ("bc_err", 16, 1), ("line", 17, 2),
     ("dist", 19, 6), ("dir", 25, 9), ("hump_jolt", 34, 8), ("tilt", 42, 9), ("roll", 51, 9),
     ("seq", 60, 4)]
INVALID = {"speed": 63, "dist": 63, "dir": 511, "hump_jolt": 255, "tilt": 511, "roll": 511}


def decode(frame: bytes):
    if len(frame) != 9:
        raise ValueError("frame is %d bytes, expected 9" % len(frame))
    if frame[0] != 1:
        raise ValueError("unknown version byte %d" % frame[0])
    w = int.from_bytes(frame[1:], "little")
    v = {n: (w >> s) & ((1 << b) - 1) for n, s, b in F}
    out = dict(v)
    for k, bad in INVALID.items():
        if v[k] == bad:
            out[k] = "invalid"
    for k in ("tilt", "roll"):
        if out[k] != "invalid":
            out[k] = v[k] - 180
    out["state"] = STATES[v["state"]]
    return out


def show(frame):
    d = decode(frame)
    return " ".join("%s=%s" % (k, d[k]) for k, _, _ in F)


def selftest():
    a = bytes([0x01, 0x01, 0x19, 0x00, 0xB5, 0x00, 0xD0, 0xA2, 0x05])
    b = bytes([0x01, 0xFF, 0xBE, 0xF7, 0xCF, 0x02, 0xA2, 0x05, 0xF0])
    da, db = decode(a), decode(b)
    assert (da["state"], da["speed"], da["dist"], da["dir"], da["tilt"], da["roll"], da["seq"]) == \
        ("LINE_FOLLOWING", 25, 32, 90, 0, 0, 0), da
    assert (db["state"], db["speed"], db["dist"], db["dir"], db["hump_jolt"], db["tilt"], db["roll"], db["seq"]) == \
        ("FAULT_STOP", 62, 62, 359, 128, 180, -180, 15), db
    print("selftest PASS (same two known frames as test_telemetry.c)")


class Tracker:
    """Prints one decoded line per frame, flags a jump in the 4-bit sequence, logs raw hex."""
    def __init__(self, log=None):
        self.prev, self.log = None, log

    def feed(self, frame: bytes):
        if self.log:
            self.log.write(frame.hex() + "\n"); self.log.flush()
        try:
            text = show(frame)
            seq = frame[8] >> 4
            gap = "" if self.prev is None or seq == (self.prev + 1) % 16 else \
                "   <-- sequence jumped (%d -> %d)" % (self.prev, seq)
            self.prev = seq
            print(len(frame), "bytes |", text + gap, flush=True)
        except ValueError as e:
            print("BAD FRAME:", e, "|", frame.hex(), flush=True)


def live(host, log):
    import paho.mqtt.client as mqtt          # pip install paho-mqtt
    t = Tracker(log)
    topic = "robocar/car1/telemetry"
    try:                                      # paho 2.x
        c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        on_connect = lambda cl, u, f, rc, p=None: cl.subscribe(topic, qos=1)
    except AttributeError:                    # paho 1.x
        c = mqtt.Client()
        on_connect = lambda cl, u, f, rc: cl.subscribe(topic, qos=1)
    c.on_connect = on_connect
    c.on_message = lambda cl, u, m: t.feed(bytes(m.payload))
    c.connect(host, 1883, 60)
    print("listening on", topic, "at", host, "(Ctrl+C to stop)", flush=True)
    c.loop_forever()


def main():
    log = open(sys.argv[sys.argv.index("--log") + 1], "a") if "--log" in sys.argv else None
    if "--selftest" in sys.argv:
        return selftest()
    if "--live" in sys.argv:
        i = sys.argv.index("--live")
        host = sys.argv[i + 1] if i + 1 < len(sys.argv) and not sys.argv[i + 1].startswith("--") else "localhost"
        return live(host, log)
    t = Tracker(log)
    for line in sys.stdin:
        h = line.strip()
        if h:
            try:
                t.feed(bytes.fromhex(h))
            except ValueError as e:
                print("BAD LINE:", e, "|", h, flush=True)


main()