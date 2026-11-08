#!/usr/bin/env python3
"""
robocar.py - watch the car's MQTT telemetry in plain English, and send commands.

Uses the Mosquitto command-line tools (mosquitto_sub / mosquitto_pub), so no
extra Python packages are needed.

  python tools/robocar.py watch                 decoded telemetry, one line per frame
  python tools/robocar.py watch --log run1.txt  same, also saved to a file
  python tools/robocar.py start                 send START (also: stop, reset)
  python tools/robocar.py decode 01...          decode one frame given as 18 hex digits
  python tools/robocar.py gui                   a window: START / STOP / RESET buttons, RUNNING or
                                                STOPPED, and the live values (needs tkinter)

Options: --host (default 192.168.137.1), --port (default 1883).

The frame layout is app_program/subsystem1/data_layout.h: 1 version byte, then
one 64-bit word sent little-endian. Keep FIELDS below in step with that file.

gui reads with the paho library if it is installed (pip install paho-mqtt), which is the
most reliable on Windows, and otherwise with mosquitto_sub like watch does.
"""
import argparse
import datetime
import os
import queue
import shutil
import subprocess
import sys
import threading
import time

TOPIC_TELEMETRY = "robocar/car1/telemetry"  # comms.h COMMS_TOPIC_TELEMETRY
TOPIC_CMD = "robocar/car1/cmd"              # comms.h COMMS_TOPIC_CMD
STALE_S = 3  # gui: no frame for this long means NO DATA
DEFAULT_HOST = "192.168.137.1"

DL_VERSION = 1
FRAME_BYTES = 9

# name: (shift, bits)  -- data_layout.h
FIELDS = {
    "state": (0, 3), "error": (3, 1), "wifi_loss": (4, 1), "low_batt": (5, 1),
    "drop": (6, 1), "hump": (7, 1), "speed": (8, 6), "bc_dir": (14, 2),
    "bc_err": (16, 1), "line": (17, 2), "dist": (19, 6), "dir": (25, 9),
    "hump_h": (34, 8), "tilt": (42, 9), "roll": (51, 9), "seq": (60, 4),
}
STATES = ["CALIBRATE", "LINE_FOLLOWING", "LINE_LOST", "BARCODE_EXECUTION",
          "LINE_SEARCH", "OBSTACLE_AVOIDANCE", "BARCODE_RESCAN", "FAULT_STOP"]
BC_DIRS = ["-", "LEFT", "RIGHT", "spare(3)"]
LINES = ["centred", "drift-right", "drift-left", "crossing"]
COMMANDS = {"start": 1, "stop": 2, "reset": 3}
COMMAND_NAMES = {v: k.upper() for k, v in COMMANDS.items()}

SPEED_INVALID, DIST_INVALID, DIR_INVALID, HUMP_H_INVALID, ANGLE_INVALID = 63, 63, 511, 255, 511
ANGLE_OFFSET = 180
JOLT_STEP_G = 0.02  # data_layout.h DL_HUMP_JOLT_STEP_MG = 20
JOLT_MAX = 128      # DL_HUMP_H_MAX_MM (now jolt steps)


def unpack(frame: bytes) -> dict:
    if len(frame) != FRAME_BYTES:
        raise ValueError(f"{len(frame)} bytes, expected {FRAME_BYTES}")
    if frame[0] != DL_VERSION:
        raise ValueError(f"version byte {frame[0]}, expected {DL_VERSION}")
    word = int.from_bytes(frame[1:], "little")
    return {k: (word >> s) & ((1 << b) - 1) for k, (s, b) in FIELDS.items()}


def compass(deg: int) -> str:
    return ["N", "NE", "E", "SE", "S", "SW", "W", "NW"][int((deg + 22.5) / 45) % 8]


def angle(v: int) -> str:
    return "n/a" if v == ANGLE_INVALID else f"{v - ANGLE_OFFSET:+d}deg"


def describe(f: dict) -> str:
    speed = "n/a" if f["speed"] == SPEED_INVALID else f"{f['speed']} cm/s"
    heading = "n/a (not calibrated)" if f["dir"] == DIR_INVALID else f"{f['dir']}deg {compass(f['dir'])}"
    dist = "n/a" if f["dist"] == DIST_INVALID else f"{f['dist']} cm"
    j = f["hump_h"]  # hump jolt, 0.02 g steps
    if j == HUMP_H_INVALID:
        jolt = "n/a"
    elif j == 0:
        jolt = "-"
    else:
        jolt = f"{'>=' if j >= JOLT_MAX else ''}{j * JOLT_STEP_G:.2f} g"

    flags = [n for n in ("error", "wifi_loss", "low_batt", "drop", "hump", "bc_err") if f[n]]
    return (f"#{f['seq']:<2} {STATES[f['state']]:<15} | speed {speed:<9} | line {LINES[f['line']]:<11} | "
            f"heading {heading:<10} | tilt {angle(f['tilt']):>5} roll {angle(f['roll']):>5} | "
            f"barcode {BC_DIRS[f['bc_dir']]:<5} | obstacle {dist} | hump jolt {jolt:<8} | "
            f"flags {' '.join(flags).upper() if flags else '-'}")


def find_tool(name: str) -> str:
    for c in (rf"C:\Program Files\mosquitto\{name}.exe", rf"C:\Program Files (x86)\mosquitto\{name}.exe"):
        if os.path.exists(c):
            return c
    path = shutil.which(name)
    if path:
        return path
    sys.exit(f"{name} not found: install Mosquitto (winget install EclipseFoundation.Mosquitto)")


def watch(args) -> None:
    cmd = [find_tool("mosquitto_sub"), "-h", args.host, "-p", str(args.port),
           "-t", TOPIC_TELEMETRY, "-t", TOPIC_CMD, "-F", "%t %x"]
    log = open(args.log, "a", encoding="utf-8") if args.log else None
    print(f"Watching {TOPIC_TELEMETRY} and {TOPIC_CMD} on {args.host}:{args.port} (Ctrl+C to stop)")

    last_seq = None
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, text=True)
    try:
        for raw in proc.stdout:
            parts = raw.split()
            if len(parts) != 2:
                continue
            topic, hexdata = parts
            now = datetime.datetime.now().strftime("%H:%M:%S")
            data = bytes.fromhex(hexdata)

            if topic == TOPIC_CMD:
                name = COMMAND_NAMES.get(data[0]) if len(data) == 1 else None
                line = f"{now}  >>> command {name or 'INVALID ' + hexdata} sent to the car"
            else:
                try:
                    f = unpack(data)
                except ValueError as e:
                    line = f"{now}  bad frame {hexdata}: {e}"
                else:
                    gap = ""
                    if last_seq is not None:
                        missed = (f["seq"] - last_seq - 1) % 16
                        if f["seq"] == last_seq:
                            gap = "  (repeat of the previous frame)"
                        elif missed:
                            gap = f"  ({missed} frame(s) missing)"
                    last_seq = f["seq"]
                    line = f"{now}  {describe(f)}{gap}"
            print(line, flush=True)
            if log:
                log.write(line + "\n")
                log.flush()
    except KeyboardInterrupt:
        pass
    finally:
        proc.terminate()
        if log:
            log.close()


def publish_command(args, action: str):
    """Sends one command byte. Returns (exit code, error text)."""
    cmd = [find_tool("mosquitto_pub"), "-h", args.host, "-p", str(args.port),
           "-t", TOPIC_CMD, "-q", "1", "-s"]
    # -s sends stdin as the message: exactly one raw byte, no newline
    r = subprocess.run(cmd, input=bytes([COMMANDS[action]]), capture_output=True)
    return r.returncode, r.stderr.decode(errors="replace").strip()


def send(args) -> None:
    rc, err = publish_command(args, args.action)
    if rc == 0:
        print(f"sent {args.action.upper()} to {TOPIC_CMD}")
    elif err:
        print(err, file=sys.stderr)
    sys.exit(rc)


def start_reader(args, put):
    """Calls put(topic, payload) for every message. Returns a function that stops reading."""
    try:
        import paho.mqtt.client as mqtt
    except ImportError:
        mqtt = None
    if mqtt:
        try:                                         # paho 2.x
            c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        except AttributeError:                       # paho 1.x
            c = mqtt.Client()
        c.on_connect = lambda cl, *a: [cl.subscribe(t, qos=1) for t in (TOPIC_TELEMETRY, TOPIC_CMD)]
        c.on_message = lambda cl, u, m: put(m.topic, bytes(m.payload))
        c.connect_async(args.host, args.port, 60)    # keeps retrying if the broker is not up yet
        c.loop_start()
        return c.loop_stop
    proc = subprocess.Popen([find_tool("mosquitto_sub"), "-h", args.host, "-p", str(args.port),
                             "-t", TOPIC_TELEMETRY, "-t", TOPIC_CMD, "-F", "%t %x"],
                            stdout=subprocess.PIPE, text=True)

    def pump():
        for raw in proc.stdout:
            parts = raw.split()
            if len(parts) == 2:
                put(parts[0], bytes.fromhex(parts[1]))
    threading.Thread(target=pump, daemon=True).start()
    return proc.terminate


def run_state(f: dict):
    """(text, colour) for the big status line, from the car's state field."""
    if f["state"] == 7:
        return "FAULT STOP", "#c62828"
    if f["state"] == 0:
        return "STOPPED", "#546e7a"
    return "RUNNING", "#2e7d32"


def gui(args) -> None:
    import tkinter as tk
    inbox = queue.Queue()
    stop_reader = start_reader(args, lambda topic, data: inbox.put(("msg", topic, data)))
    root = tk.Tk()
    root.title("Robocar dashboard")
    root.minsize(420, 0)
    status = tk.Label(root, text="NO DATA", width=18, fg="white", bg="#ef6c00", font=("Segoe UI", 22, "bold"))
    sent = tk.StringVar(value="no command sent yet")
    body = tk.StringVar()
    last = {"t": 0.0}

    def press(name):
        sent.set(f"sending {name.upper()}...")

        def work():
            rc, err = publish_command(args, name)
            inbox.put(("sent", name, rc, err))
        threading.Thread(target=work, daemon=True).start()

    status.pack(fill="x", padx=12, pady=(12, 6))
    row = tk.Frame(root)
    row.pack(pady=6)
    for name in COMMANDS:
        tk.Button(row, text=name.upper(), width=10, command=lambda n=name: press(n)).pack(side="left", padx=4)
    tk.Label(root, textvariable=sent).pack()
    tk.Label(root, textvariable=body, font=("Consolas", 10), justify="left", anchor="w").pack(fill="x", padx=24, pady=10)

    def tick():
        while not inbox.empty():
            item = inbox.get()
            if item[0] == "sent":
                _, name, rc, err = item
                sent.set(f"sent {name.upper()} at {time.strftime('%H:%M:%S')}" if rc == 0
                         else f"FAILED to send {name.upper()}: {err or 'mosquitto_pub error'}")
            elif item[1] == TOPIC_TELEMETRY:
                try:
                    f = unpack(item[2])
                except ValueError as e:
                    body.set(f"bad frame: {e}")
                    continue
                last["t"] = time.time()
                text, colour = run_state(f)
                status.config(text=text, bg=colour)
                body.set("\n".join(describe(f).split(" | ")))
        if time.time() - last["t"] > STALE_S:
            status.config(text="NO DATA", bg="#ef6c00")
        root.after(200, tick)

    tick()
    root.mainloop()
    stop_reader()


def main() -> None:
    p = argparse.ArgumentParser(description="Robo Pico car MQTT tool")
    p.add_argument("action", choices=["watch", "decode", "gui", *COMMANDS])
    p.add_argument("hex", nargs="?", help="frame as hex digits (decode only)")
    p.add_argument("--host", default=DEFAULT_HOST)
    p.add_argument("--port", type=int, default=1883)
    p.add_argument("--log", help="also append decoded lines to this file (watch only)")
    args = p.parse_args()

    if args.action == "watch":
        watch(args)
    elif args.action == "gui":
        gui(args)
    elif args.action == "decode":
        if not args.hex:
            p.error("decode needs the frame, e.g. decode 01410a...")
        print(describe(unpack(bytes.fromhex(args.hex))))
    else:
        send(args)


if __name__ == "__main__":
    main()