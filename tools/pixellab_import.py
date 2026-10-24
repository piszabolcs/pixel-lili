#!/usr/bin/env python3
"""Pixel Lili: PixelLab editor exports -> frames + previews (2026-10-23, the 2D line).

    py -3.12 tools/pixellab_import.py walk_S [walk_SW ...]
    py -3.12 tools/pixellab_import.py --combo walk_all walk_S walk_SW ...   (side by side, one GIF)

assets/pixellab/<tag>_sheet.png (the editor's spritesheet export: one row of square frames, 96 px or, from the
2026-10-23 night batch on, 128 px (the frame size is the sheet's height), saved through the
local receiver, no Save dialog; tracked in git, because a generation costs credits and cannot be redone for free) ->
  assets/raw_renders/pixellab/<tag>/f000.png ...   every frame as it came (frame 0 = the source pose)
  docs/img/anim/pixellab_<tag>.gif                  4x, 110 ms, the source pose once, then the generated frames
  docs/img/anim/pixellab_<tag>_frames.png           4x contact sheet, 5 per row
Previews only: nothing goes into assets/sprites/lili or the pak until the owner approves it.
Animate with Text (New) returns the source twice (frame 0 and 1); the duplicate is dropped from the GIF.
A job in assets/pixellab/jobs.json with "makes": its last frame is also saved as assets/startframes/<makes>.png,
the start frame of later jobs (a sit-down's last frame is the sitting pose Gemini did not draw).
"""
import json, os, sys
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RAW = os.path.join(ROOT, "assets", "raw_renders", "pixellab")
DOC = os.path.join(ROOT, "docs", "img", "anim")
BG = (40, 44, 52)
S = 4                                   # preview scale (3 for the 128 px frames)

def big(f):
    s = 3 if f.width > 96 else S
    b = Image.new("RGBA", f.size, BG + (255,)); b.alpha_composite(f)
    return b.convert("RGB").resize((f.width * s, f.height * s), Image.NEAREST)

def run(tag):
    sheet = Image.open(os.path.join(ROOT, "assets", "pixellab", tag + "_sheet.png")).convert("RGBA")
    CELL = sheet.height
    frames = [sheet.crop((i * CELL, 0, (i + 1) * CELL, CELL)) for i in range(sheet.width // CELL)]
    out = os.path.join(RAW, tag); os.makedirs(out, exist_ok=True)
    for i, f in enumerate(frames):
        f.save(os.path.join(out, f"f{i:03d}.png"))
    # the editor sometimes appends the generated frames after its empty placeholders: drop empty frames too
    seq = frames[:1] + [f for i, f in enumerate(frames[1:], 1)
                        if not np.array_equal(np.asarray(f), np.asarray(frames[0])) and np.asarray(f)[..., 3].any()]
    bigs = [big(f) for f in seq]
    bigs[0].save(os.path.join(DOC, f"pixellab_{tag}.gif"), save_all=True, append_images=bigs[1:], duration=110, loop=0)
    rows = (len(bigs) + 4) // 5; T = bigs[0].width
    cs = Image.new("RGB", (5 * T, rows * T), BG)
    for i, b in enumerate(bigs):
        cs.paste(b, ((i % 5) * T, (i // 5) * T))
    cs.save(os.path.join(DOC, f"pixellab_{tag}_frames.png"))
    print(f"{tag}: {len(frames)} frames in the sheet, {len(seq) - 1} generated -> docs/img/anim/pixellab_{tag}.gif")
    jobs = os.path.join(ROOT, "assets", "pixellab", "jobs.json")
    if os.path.exists(jobs):
        for j in json.load(open(jobs, encoding="utf-8"))["jobs"]:
            if j["tag"] == tag and j.get("makes"):
                seq[-1].save(os.path.join(ROOT, "assets", "startframes", j["makes"] + ".png"))
                print(f"  its last frame -> assets/startframes/{j['makes']}.png")

def combo(name, tags):
    """one GIF with several imported animations side by side (e.g. every direction of the walk)"""
    seqs = []
    for t in tags:
        d = os.path.join(RAW, t)
        fr = [Image.open(os.path.join(d, f)).convert("RGBA") for f in sorted(os.listdir(d)) if f.endswith(".png")]
        seqs.append(fr[:1] + [f for f in fr[1:] if not np.array_equal(np.asarray(f), np.asarray(fr[0]))])
    n = max(len(s) for s in seqs); T = max(big(s[0]).width for s in seqs)
    out = []
    for i in range(n):
        row = Image.new("RGB", (len(seqs) * T, T), BG)
        for k, s in enumerate(seqs):
            b = big(s[i % len(s)]); row.paste(b, (k * T + (T - b.width) // 2, T - b.height))
        out.append(row)
    out[0].save(os.path.join(DOC, f"pixellab_{name}.gif"), save_all=True, append_images=out[1:], duration=110, loop=0)
    print(f"{name}: {', '.join(tags)} -> docs/img/anim/pixellab_{name}.gif")

if __name__ == "__main__":
    if sys.argv[1] == "--combo":
        combo(sys.argv[2], sys.argv[3:])
    else:
        for t in sys.argv[1:]:
            run(t)
