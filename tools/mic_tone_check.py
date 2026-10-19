#!/usr/bin/env python3
"""Pixel Lili: check the panel's microphone chain against known tones from the PC speaker.

    py -3.12 tools/mic_tone_check.py [--port COM3] [--freqs 1250,2000,3000] [--amp 0.5]

Plays each tone for 1.5 s (with 1 s of silence between) through the PC's
default audio output while reading the diag console. Every steady tone in the
1000-3500 Hz band trips the whistle detector, whose event line carries the
frequency it measured; a wrong I2S / ES7210 clock setup would scale every
reading by the same factor. Also dumps the ES7210 registers (`mic regs`).
Windows only (winsound); the panel should sit near the speaker.
"""
import argparse, math, os, re, struct, sys, tempfile, threading, time, wave, winsound
import serial

RATE = 48000

def tone_wav(freqs, amp, path):
    frames = bytearray()
    for f in freqs:
        n = int(1.5 * RATE)
        for i in range(n):
            env = min(1.0, i / 480, (n - i) / 480)            # 10 ms fades: no clicks
            frames += struct.pack("<h", int(32767 * amp * env * math.sin(2 * math.pi * f * i / RATE)))
        frames += b"\x00\x00" * RATE                           # 1 s of silence
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(RATE); w.writeframes(bytes(frames))

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default="COM3")
    ap.add_argument("--freqs", default="1250,2000,3000")
    ap.add_argument("--amp", type=float, default=0.5)
    a = ap.parse_args()
    freqs = [float(x) for x in a.freqs.split(",")]
    path = os.path.join(tempfile.gettempdir(), "lili_tones.wav")
    tone_wav(freqs, a.amp, path)

    s = serial.Serial(); s.port = a.port; s.baudrate = 115200; s.timeout = 0.1
    s.dtr = False; s.rts = False; s.open()
    time.sleep(0.3); s.reset_input_buffer()
    s.write(b"mic regs\n"); s.flush()
    log = []; stop = threading.Event()
    def reader():
        buf = ""
        while not stop.is_set():
            buf += s.read(512).decode("utf-8", "replace")
            while "\n" in buf:
                line, buf = buf.split("\n", 1)
                log.append((time.time(), re.sub(r"\x1b\[[0-9;]*m", "", line).strip()))
    th = threading.Thread(target=reader, daemon=True); th.start()
    time.sleep(1.0)
    t0 = time.time()
    winsound.PlaySound(path, winsound.SND_FILENAME)            # blocks for the whole sequence
    time.sleep(1.5); stop.set(); th.join(); s.close()

    for t, l in log:
        if "ES7210 regs" in l: print(l[l.index("ES7210"):])
    print(f"{'played Hz':>10} {'measured Hz':>12} {'error':>8}   (bin = 31.25 Hz)")
    events = [(t - t0, l) for t, l in log if "FÜTTY #" in l and t >= t0]
    for k, f in enumerate(freqs):
        lo, hi = k * 2.5, k * 2.5 + 2.4                         # this tone's window, s after the start
        hits = [l for dt, l in events if lo <= dt <= hi]
        if not hits:
            print(f"{f:>10.0f} {'-':>12} {'no event':>8}"); continue
        m = float(re.search(r"#\d+: (\d+) Hz", hits[0]).group(1))
        lvl = re.search(r"peak (-?[\d.]+) dBFS", hits[0]).group(1)
        print(f"{f:>10.0f} {m:>12.0f} {m - f:>+8.0f}   peak {lvl} dBFS")
    stat = [l for t, l in log if "diag:" in l and "mic L" in l]
    if stat: print("last status:", stat[-1][stat[-1].index("mic L"):][:90])

if __name__ == "__main__":
    main()
