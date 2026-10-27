#!/usr/bin/env python3
"""Pixel Lili: how big the pak gets, measured, against the limits it has to fit (2026-10-27, before M3.5).

    py -3.12 tools/pak_budget.py            every approved PixelLab animation at the room's and the yard's size

The owner (2026-10-27): do we fit at all? Each approved animation (the review page's set: docs/review, without the
hidden ones, a variant only where the plain one is missing) goes through the game's conversion (tools/pixellab_norm.py
one size and ground line, tools/pixellab_to_sprites.py the palette and the two sizes) and is RLE-packed like
tools/pack_assets.py does, with its 12-byte frame record. The backgrounds are read from the current pak.
The cells are cropped like the game's (tools/pixellab_to_sprites.py convert), each set with its 12-byte header.
The limits: the app partition (the pak is linked into the firmware image today), the assets partition of spec 5.10,
the pak's 3 MB target (M3.8) and the 2 MB it was given in PSRAM (spec 5.11).
"""
import json, os, struct, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import pixellab_norm as pn
import pixellab_to_sprites as ps
from pack_assets import rle

# spec 7.4: the first round (M3.5); sneak is A07's tiptoe, sit_down plays backwards as stand_up
FIRST_ROUND = ("idle_stand", "idle_sit_blink", "sit_down", "walk", "trot", "run", "sneak", "scurry", "pant_smile")
HIDDEN = {"drink_W", "bark_S"}                      # tools/review_page.py: rejected, still in the folder
PREFER = ("", "pmm", "v3", "v2", "try1")            # per direction: the plain sheet, else the variant the owner kept
APP = 0x280000                                      # firmware/lili/partitions.csv factory
FW_CODE = 1_760_032 - 1_460_441                     # docs/stats.md 2026-10-23: the image minus the pak linked into it
ASSETS = 0x1000000 - 0xA90000                       # spec 5.10: the assets partition, to the end of the 16 MB flash
TARGET, PSRAM = 3 * 2**20, 2 * 2**20                # spec M3.8 and 5.11

def approved():
    sheets = [f[:-10] for f in os.listdir(os.path.join(ROOT, "assets", "pixellab")) if f.endswith("_sheet.png") and not f.startswith("yard_")]
    pick = {}
    for t in sorted(sheets):
        name, d, var = pn.split(t)
        if t in HIDDEN or var not in PREFER: continue
        if (name, d) not in pick or PREFER.index(var) < PREFER.index(pn.split(pick[name, d])[2]): pick[name, d] = t
    return pick

def backgrounds():
    b = open(os.path.join(ROOT, "assets", "build", "assets.pak"), "rb").read()
    _, _, n, toc, _, _ = struct.unpack_from("<4sHHIII", b, 0)
    return sum(struct.unpack_from("<HBBII", b, toc + 12 * i)[4] for i in range(n) if struct.unpack_from("<HBBII", b, toc + 12 * i)[1] != 1)

def main():
    ctx = ps.context()
    jobs = json.load(open(os.path.join(ROOT, "assets", "pixellab", "jobs.json"), encoding="utf-8"))["jobs"]
    by_anim = {}
    for (name, d), tag in sorted(approved().items()): by_anim.setdefault(name, {})[d] = tag
    per = {}                                        # anim -> {"room": B, "yard": B, "frames": n, "dirs": [..]}
    for name, src in by_anim.items():
        sets = ps.convert(name, src, ctx, jobs)     # the game's own conversion: normalized, both sizes, cropped cells
        a = per[name] = {"frames": sum(len(s) for s in sets["master"][0].values()), "dirs": list(src)}
        for size, sub in (("room", "lili"), ("yard", "lili_yard")):
            a[size] = sum(len(rle(f.flatten())) + 12 for s in sets[sub][0].values() for f in s) + 12
    bg = backgrounds()
    first = {k: sum(v[k] for n, v in per.items() if n in FIRST_ROUND) for k in ("room", "yard")}
    every = {k: sum(v[k] for v in per.values()) for k in ("room", "yard")}
    kb = lambda b: f"{b / 1024:7.0f} KB"
    print(f"{'animation':16s} {'dirs':>4s} {'frames':>6s} {'room':>10s} {'yard':>10s}")
    for name, v in sorted(per.items(), key=lambda kv: -(kv[1]["room"] + kv[1]["yard"])):
        mark = " *" if name in FIRST_ROUND else ""
        print(f"{name:16s} {len(v['dirs']):4d} {v['frames']:6d} {kb(v['room'])} {kb(v['yard'])}{mark}")
    print(f"(* = M3.5, A01-A09)  frames: {sum(v['frames'] for v in per.values())}, "
          f"{every['room'] / sum(v['frames'] for v in per.values()):.0f} B a frame in the room")
    rows = (("backgrounds (painted rooms, covers, light masks)", bg),
            ("M3.5 sprites, both sizes", first["room"] + first["yard"]),
            ("M3.5 pak: backgrounds + M3.5 sprites", bg + first["room"] + first["yard"]),
            ("every approved sprite, both sizes", every["room"] + every["yard"]),
            ("the whole pak: backgrounds + every sprite", bg + every["room"] + every["yard"]))
    for label, b in rows: print(f"{label:48s} {kb(b)}")
    print("limits:")
    for label, lim in ((f"pak linked into the app (2.5 MB partition minus {FW_CODE // 1024} KB code)", APP - FW_CODE),
                       ("the pak target (spec M3.8)", TARGET), ("the pak in PSRAM (spec 5.11)", PSRAM),
                       ("the assets partition (spec 5.10)", ASSETS)):
        m35, whole = bg + first["room"] + first["yard"], bg + every["room"] + every["yard"]
        print(f"  {label:58s} {kb(lim)}   M3.5 {'fits' if m35 <= lim else 'OVER'}, whole {'fits' if whole <= lim else 'OVER'}")

if __name__ == "__main__":
    main()
