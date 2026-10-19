#!/usr/bin/env python3
"""Pixel Lili: set the panel's PCF85063 from the PC clock, or measure its offset.

    py -3.12 tools/rtc_sync.py            set it (aligned to the PC's second boundary), then measure
    py -3.12 tools/rtc_sync.py --check    measure only (for drift: run it hours later)

Needs the diag firmware (its `settime` and `time` console commands). The RTC
keeps local wall time (spec 5.6), so the PC's local time is what gets written.
The PCF85063 counts whole seconds, so the set is aligned: we wait until the PC
clock is just past a second boundary and send the time of that second. What is
left is the serial latency (a few ms) plus the chip's sub-second phase, which a
whole-second register cannot show: the --check result is good to +-1 s.
"""
import argparse, datetime as dt, re, sys, time
import serial

def open_port(port):
    s = serial.Serial(); s.port = port; s.baudrate = 115200; s.timeout = 0.1
    s.dtr = False; s.rts = False; s.open()
    return s

def send(s, cmd):
    s.write((cmd + "\n").encode()); s.flush()

def read_rtc(s, wait=1.5):
    """sends `time`, returns (rtc datetime, pc datetime at the reply, valid flag)"""
    s.reset_input_buffer(); send(s, "time")
    end = time.time() + wait; buf = ""
    while time.time() < end:
        buf += s.read(512).decode("utf-8", "replace")
        m = re.search(r"RTC (\d{4}-\d\d-\d\d \d\d:\d\d:\d\d) \((valid|INVALID)", buf)
        if m:
            pc = dt.datetime.now()
            return dt.datetime.strptime(m.group(1), "%Y-%m-%d %H:%M:%S"), pc, m.group(2) == "valid"
    sys.exit("no `time` reply: is the diag firmware running, and is the port free?")

def measure(s, label):
    # sample a few times: the reply's whole second against the PC's fractional time
    offs = []
    for _ in range(5):
        rtc, pc, valid = read_rtc(s)
        offs.append((rtc - pc).total_seconds())
        time.sleep(0.3)
    print(f"{label}: RTC {rtc}  PC {pc:%Y-%m-%d %H:%M:%S.%f}"[:-3] + f"  valid={valid}")
    print(f"{label}: offset RTC - PC = {min(offs):+.2f} .. {max(offs):+.2f} s (whole-second register, +-1 s)")

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default="COM3")
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    s = open_port(a.port)
    time.sleep(0.3); s.reset_input_buffer()
    measure(s, "before")
    if not a.check:
        now = dt.datetime.now()
        time.sleep(1.0 - now.microsecond / 1e6 + 0.005)       # 5 ms past the next second boundary
        t = dt.datetime.now().replace(microsecond=0)
        send(s, f"settime {t:%Y-%m-%dT%H:%M:%S}")
        print(f"sent settime {t:%Y-%m-%dT%H:%M:%S} at PC {dt.datetime.now():%H:%M:%S.%f}"[:-3])
        time.sleep(1.5)
        measure(s, "after")
    s.close()

if __name__ == "__main__":
    main()
