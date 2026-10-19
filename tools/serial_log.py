#!/usr/bin/env python3
"""Pixel Lili: read the panel's USB serial console from Windows, send commands, keep a log.

    py -3.12 tools/serial_log.py --seconds 20
    py -3.12 tools/serial_log.py --seconds 30 --send "help;time" --send-delay 2
    py -3.12 tools/serial_log.py --seconds 600 --log docs/logs/2026-10-18_diag.log

Opens the port with DTR/RTS low (so the ESP32-S3's USB-Serial-JTAG is not reset
into the bootloader), strips the log colours, stamps each line, and reconnects
when the port vanishes (the console drops in light sleep) until the time is up.
Commands in --send are separated by ';' and written one per second after
--send-delay seconds. Needs pyserial (py -3.12 -m pip install --user pyserial).
"""
import argparse, re, sys, time
import serial
from serial.serialutil import SerialException

ANSI = re.compile(r"\x1b\[[0-9;]*m")

def open_port(port, baud):
    s = serial.Serial()
    s.port = port; s.baudrate = baud; s.timeout = 0.2
    s.dtr = False; s.rts = False
    s.open()
    return s

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default="COM3")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--seconds", type=float, default=15)
    ap.add_argument("--send", default="", help="commands separated by ';'")
    ap.add_argument("--send-delay", type=float, default=1.5)
    ap.add_argument("--log", default="")
    ap.add_argument("--grep", default="", help="print only lines matching this regex (the log keeps all)")
    a = ap.parse_args()
    cmds = [c.strip() for c in a.send.split(";") if c.strip()]
    log = open(a.log, "a", encoding="utf-8", buffering=1) if a.log else None   # line-buffered: a stopped run keeps every line
    pat = re.compile(a.grep) if a.grep else None
    t_end = time.time() + a.seconds
    t_send = time.time() + a.send_delay
    s = None; buf = b""; n = 0
    while time.time() < t_end:
        if s is None:
            try:
                s = open_port(a.port, a.baud)
                line = f"--- {time.strftime('%H:%M:%S')} port {a.port} open"
                print(line); log and log.write(line + "\n")
            except SerialException:
                time.sleep(0.5); continue
        try:
            if cmds and time.time() >= t_send:
                c = cmds.pop(0)
                s.write((c + "\n").encode()); s.flush()
                line = f">>> {c}"
                print(line); log and log.write(line + "\n")
                t_send = time.time() + 1.0
            data = s.read(4096)
        except SerialException:
            line = f"--- {time.strftime('%H:%M:%S')} port lost (sleep?), retrying"
            print(line); log and log.write(line + "\n")
            try: s.close()
            except Exception: pass
            s = None; time.sleep(0.5); continue
        if not data:
            continue
        buf += data
        while b"\n" in buf:
            raw, buf = buf.split(b"\n", 1)
            text = ANSI.sub("", raw.decode("utf-8", "replace")).rstrip("\r")
            stamped = f"{time.strftime('%H:%M:%S')} {text}"
            n += 1
            if log: log.write(stamped + "\n")
            if pat is None or pat.search(text):
                print(stamped)
    if s is not None:
        s.close()
    if log:
        log.close()
    print(f"--- done: {n} lines")

if __name__ == "__main__":
    main()
