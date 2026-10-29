#!/usr/bin/env python3
"""Pixel Lili: her sound bank (2026-10-29, spec 11.1 / M9.2) from the ElevenLabs takes the owner picked.

    py -3.12 tools/lili_sounds.py         then tools/pack_assets.py (it packs the bank and writes the SFX_* ids)

assets/sounds/elevenlabs/<take>_<n>.ogg   the generated takes (tools/sound_recv.py), prompts.tsv says how
assets/sounds/elevenlabs/picks.tsv        the owner's picks: take -> the numbers kept ("-" = none)
assets/sounds/lili/bank.bin, bank.json    the bank the pack carries (PAK_T_SOUND 1) and what is in it

Each picked take: mono, 16 kHz (ffmpeg, in WSL), a 350 Hz high-pass (the 12 mm speaker says nothing below ~600 Hz;
350 keeps a little of the bark's body for the phone-sized sim) and a 7 kHz low-pass, the silence under -50 dBFS
trimmed at both ends, 5 ms fades, peaked at -1 dBFS, then G.711 mu-law: one byte a sample, so a minute is under
1 MB (the pack's partition had 1.3 MB left; PCM16 would be 1.9 MB). The cue's gain and cooldown are below.

bank.bin (little-endian): "LSND", u16 version 1, u16 cues, u16 clips, u16 rate;
  cues[]: u8 first clip, u8 variants, u8 loop, u8 0, u16 gain (q8, 256 = 0 dB), u16 cooldown ms, char name[12];
  clips[]: u32 offset, u32 bytes (into the data); then the mu-law data.
"""
import csv, json, os, struct, subprocess, sys
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets", "sounds", "elevenlabs")
OUT = os.path.join(ROOT, "assets", "sounds", "lili")
RATE = 16000
# the game's cues: its takes (the picks of these), gain in dB, cooldown in ms. Order = the SFX_* ids.
CUES = [
    ("bark",        ["bark_a", "bark_b", "bark_c"], 0, 400),
    ("whimper",     ["whimper_a"],      -3, 1500),
    ("snort",       ["snort_b"],        -3, 1500),
    ("sniff",       ["sniff_a"],        -6, 1500),
    ("huff",        ["huff_b"],         -2, 600),
    ("pant",        ["pant_a"],        -12, 1500),
    ("snore",       ["snore_a"],       -18, 3000),
    ("whine",       ["whine_a"],        -4, 2500),
    ("yelp",        ["yelp_a"],          0, 800),
    ("growl",       ["growl_a"],        -4, 1800),
    ("yawn",        ["yawn_a"],         -6, 3000),
    ("eat",         ["eat_a"],          -6, 1800),
    ("drink",       ["drink_a"],        -6, 1800),
    ("crunch",      ["crunch_a"],       -4, 800),
    ("chew",        ["chew_a"],         -8, 1800),
    ("rattle",      ["rattle_a"],       -8, 400),
    ("kibble_pour", ["kibble_pour_b"],  -6, 500),
    ("water_pour",  ["water_pour_b"],   -6, 500),
    ("wipe",        ["wipe_a"],         -6, 300),
    ("pad_change",  ["pad_change_a"],   -6, 500),
    ("treat_drop",  ["treat_drop_a"],   -6, 200),
    ("toy_land",    ["toy_land_b"],     -8, 200),
    ("pet_door",    ["pet_door_a"],     -8, 500),
]

def picks():
    out = {}
    with open(os.path.join(SRC, "picks.tsv"), encoding="utf-8") as f:
        for row in list(csv.reader(f, delimiter="\t"))[1:]:
            if len(row) >= 2 and row[1].strip() != "-":
                out[row[0]] = [int(n) for n in row[1].split()]
    return out

def decode(path):
    wsl = "/mnt/" + path[0].lower() + path[2:].replace("\\", "/")
    r = subprocess.run(["wsl", "-d", "Ubuntu-24.04", "--", "ffmpeg", "-loglevel", "error", "-i", wsl, "-ac", "1", "-ar", str(RATE),
                        "-af", "highpass=f=350,lowpass=f=7000", "-f", "s16le", "-"], capture_output=True, check=True)
    return np.frombuffer(r.stdout, np.int16).astype(np.float32) / 32768

def tidy(x):
    hop = RATE // 100
    rms = np.array([np.sqrt((x[i:i + hop] ** 2).mean() + 1e-12) for i in range(0, len(x), hop)])
    loud = np.nonzero(20 * np.log10(rms) > -50)[0]
    if not len(loud): return x[:0]
    x = x[max(0, loud[0] - 1) * hop:min(len(x), (loud[-1] + 2) * hop)].copy()
    f = int(0.005 * RATE); ramp = np.linspace(0, 1, f)
    x[:f] *= ramp; x[-f:] *= ramp[::-1]
    return x * (10 ** (-1 / 20) / max(1e-6, np.abs(x).max()))

def mulaw(x):
    """G.711 mu-law of floats in -1..1 (the C side decodes with the standard table)"""
    s = np.clip(np.round(x * 32767), -32767, 32767).astype(np.int32)
    sign = np.where(s < 0, 0x80, 0); m = np.minimum(np.abs(s), 32635) + 0x84
    exp = np.floor(np.log2(m)).astype(np.int32) - 7
    mant = (m >> (exp + 3)) & 0x0F
    return (~(sign | (exp << 4) | mant) & 0xFF).astype(np.uint8)

def main():
    pk = picks(); os.makedirs(OUT, exist_ok=True)
    clips, cues, data, man = [], [], bytearray(), []
    for name, takes, db, cool in CUES:
        first = len(clips); used = []
        for t in takes:
            for n in pk.get(t, []):
                x = tidy(decode(os.path.join(SRC, f"{t}_{n}.ogg")))
                if not len(x): continue
                b = mulaw(x); clips.append((len(data), len(b))); data += b.tobytes(); used.append(f"{t}_{n} {len(x) / RATE:.2f}s")
        cues.append((first, len(clips) - first, 0, int(round(256 * 10 ** (db / 20))), cool, name))
        man.append({"cue": name, "takes": used, "gain_db": db, "cooldown_ms": cool})
        print(f"{name:12s} {len(used)} take(s): {', '.join(used) if used else '(none picked yet: silent)'}")
    blob = struct.pack("<4sHHHH", b"LSND", 1, len(cues), len(clips), RATE)
    for first, n, loop, gain, cool, name in cues:
        blob += struct.pack("<BBBBHH12s", first, n, loop, 0, gain, cool, name.encode())
    for off, n in clips:
        blob += struct.pack("<II", off, n)
    blob += bytes(data)
    open(os.path.join(OUT, "bank.bin"), "wb").write(blob)
    json.dump({"rate": RATE, "format": "mu-law", "cues": man}, open(os.path.join(OUT, "bank.json"), "w", encoding="utf-8"), indent=1, ensure_ascii=False)
    print(f"-> assets/sounds/lili/bank.bin {len(blob)} B ({len(data) / RATE:.1f} s in {len(clips)} clips, {len(cues)} cues)")

if __name__ == "__main__":
    main()
