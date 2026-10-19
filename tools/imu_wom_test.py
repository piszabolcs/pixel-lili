#!/usr/bin/env python3
"""Pixel Lili: M1.4 test - does the QMI8658's wake-on-motion reach the ESP32-S3 (INT1 = GPIO 17)?

    py -3.12 tools/imu_wom_test.py [--port COM3] [--log docs/logs/...log]

Paced by the person holding the panel, not by a clock:
  1. awake: `wom on` arms the WoM; the diag polls GPIO 17 and logs every event.
     The script waits until 3 events (pick it up / put it down 3 times).
  2. asleep: `womsleep 300` turns the panel off and enters light sleep with
     GPIO 17 (and PWR) as wake sources. The USB console drops while it sleeps;
     the script reconnects and waits for the diag's `wake:` line (pick it up).
Reports the INT1 level before arming, the awake events and the wake reason.
"""
import argparse, re, time
import serial
from serial.serialutil import SerialException

def opener(port):
    while True:
        try:
            s = serial.Serial(); s.port = port; s.baudrate = 115200; s.timeout = 0.2
            s.dtr = False; s.rts = False; s.open(); return s
        except SerialException:
            time.sleep(0.5)

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--port", default="COM3"); ap.add_argument("--log")
    a = ap.parse_args()
    log = open(a.log, "a", encoding="utf-8", buffering=1) if a.log else None
    def out(line):
        stamped = time.strftime("%H:%M:%S ") + line
        print(stamped, flush=True)
        if log: log.write(stamped + "\n")
    s = opener(a.port); time.sleep(0.3); s.reset_input_buffer()
    buf = ""
    def lines(wait):
        nonlocal s, buf
        end = time.time() + wait
        while time.time() < end:
            try:
                buf += s.read(512).decode("utf-8", "replace")
            except SerialException:
                out("--- port gone (light sleep?), reconnecting"); s.close(); s = opener(a.port); out("--- port back")
                continue
            while "\n" in buf:
                l, buf = buf.split("\n", 1); l = re.sub(r"\x1b\[[0-9;]*m", "", l).strip()
                if l: yield l
    def send(c): s.write((c + "\n").encode()); s.flush(); out(">>> " + c)

    send("imu")
    for l in lines(2):
        if "imu " in l or "INT1" in l: out(l)
    send("wom on 100 8")
    events = 0; t0 = time.time()
    while events < 3 and time.time() - t0 < 300:
        for l in lines(1):
            if "WoM" in l or "imu:" in l: out(l)
            if "WoM event #" in l: events += 1
    out(f"awake: {events} WoM events in {time.time() - t0:.0f} s")
    if events == 0:
        out("no awake event: INT1 does not reach GPIO 17 (or the WoM is not armed) - stopping"); return
    time.sleep(2); send("womsleep 300")
    t1 = time.time(); woke = None
    while time.time() - t1 < 330 and not woke:
        for l in lines(1):
            if any(k in l for k in ("sleep (", "WoM", "wake:", "INT1")): out(l)
            if re.search(r"diag: wake: ", l): woke = l      # not "awake:" (the WoM event lines contain it)
    out("RESULT: " + (woke or "no wake line within 330 s"))

if __name__ == "__main__":
    main()
