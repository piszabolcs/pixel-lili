#!/usr/bin/env python3
"""Pixel Lili: one size and one ground line for every PixelLab animation (2026-10-26, the owner).

    py -3.12 tools/pixellab_norm.py --startframes   assets/startframes: stand_N's tail hole cleared, the odd sizes to the pose's height
    py -3.12 tools/pixellab_norm.py --check         docs/img/anim/size_check.png: every animation's first and last frame on one ground line

The owner (2026-10-26): the dogs were bigger and smaller and shifted about between the animations; when they are
put together, nothing may jump or be 20-30 % bigger. Measured (the silhouette's height in the 128 px cell):
stand_up_NW grew the dog 20 % (95 px against the standing masters' 79), so every NW animation made from its last
frame was 20 % too big; some Gemini cells were drawn small (sit_NE 72, sit_W 76 against sit_S's 81); trot_S and
sniff_E rose 26-33 px off the ground while moving in place; the old 96 px sheets showed 4/3 bigger on the review
page. The height is a fair measure only for standing and sitting (the masters exist, the ears and the head make
the top in every direction); a bow's or a carry's top is the tail or the toy, so those keep their size.
Per animation, normalize():
  1. a 96 px sheet goes into the 128 px cell (the same dog: pivot (48, 90) -> (64, 122))
  2. in the animations made from stand_N (HOLES), the light patch the tail's curl encloses or its gap holds is
     cleared; in every animation, the stray dots of a few pixels go
  3. one scale from the first frame: the pose's height (REF) when it is off by more than TOL; a transition
     (sit down, stand up) ramps to its end pose's height over the frames, so it meets the next animation
  4. every frame's lowest pixel on the row above the pivot (in place, nothing floats), except the jumps
     (startle, pounce), which keep their own up and down and only start on the ground
"""
import json, os, re, sys
import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CELL, PIVOT = 128, (64, 122)
REF = {"stand": 79, "sit": 81}            # the masters: assets/sprites/lili_src stand_* 78-79, sit_S 81
# the other poses' start frames were cut from the owner's Gemini sheets at one scale (tools/startframes_cut.py), but
# Gemini drew some sheets bigger (measured 2026-10-27, the owner: she grew when she bowed or picked up the rope): sheet
# 02's standing dog (stand_E2) was 89.7 px against the masters' 79, so every bow is 0.881; sheet 03's carries stand
# 8 % taller than the masters in their directions (median), 0.92; sheet 05's dogs match the masters' heads at 1.06
# (the head matched on the front views), 0.943. Sheets 01, 04 and 06 (06 holds the scale's reference) are 1.
SHEET_SIZE = {"02": 0.881, "03": 0.92, "05": 0.943}
CELLS = os.path.join(ROOT, "docs", "art", "startframes", "cells.json")
TOL = 0.03
END = {"sit_down": "sit", "stand_up": "stand", "lie_down": "lie", "squat_pee": "pee"}
JUMP = ("startle", "pounce")
# the animations older than jobs.json (2026-10-23): what they start from
POSE_OF = {"walk": "stand", "idle_stand": "stand", "look_around": "stand", "idle_sit_blink": "sit",
           "pant_smile": "sit", "play_bow": "bow", "sleep_curl": "curl", "eat": "eat", "drink": "drink",
           "run": "stand", "bark": "stand", "carry_trot": "carry"}
VARIANT = re.compile(r"_(pmm|v\d+|try\d+)$")
DIRS = ("S", "SW", "W", "NW", "N", "NE", "E", "SE")

def split(tag):
    """walk_NE -> ("walk", "NE", ""), scurry_SW_try1 -> ("scurry", "SW", "try1")"""
    m = VARIANT.search(tag); var = m.group(1) if m else ""
    base = tag[:m.start()] if m else tag
    name, _, d = base.rpartition("_")
    return (name, d, var) if d in DIRS else (base, "", var)

def job_for(tag, jobs):
    name, d, var = split(tag)
    base = f"{name}_{d}" if d else name
    for j in jobs:
        if j["tag"] == base: return dict(j, tag=tag)
    return {"tag": tag, "start": f"{POSE_OF.get(name, name)}_{d}", "action": ""}

def sheet_frames(tag):
    """the sheet's frames as the importer keeps them: the source pose, then the generated ones (no duplicates, no blanks)"""
    s = np.asarray(Image.open(os.path.join(ROOT, "assets", "pixellab", tag + "_sheet.png")).convert("RGBA"))
    c = s.shape[0]
    cells = [s[:, i * c:(i + 1) * c] for i in range(s.shape[1] // c)]
    return cells[:1] + [f for f in cells[1:] if f[..., 3].any() and not np.array_equal(f, cells[0])]

def to128(f):
    if f.shape[0] == CELL: return f
    out = np.zeros((CELL, CELL, 4), np.uint8)
    ox, oy = PIVOT[0] - 48, PIVOT[1] - 90                      # the 96 px cell's pivot (48, 90)
    out[oy:oy + f.shape[0], ox:ox + f.shape[1]] = f
    return out

def height(f):
    ys = np.nonzero((f[..., 3] > 0).any(1))[0]
    return int(ys.max() - ys.min() + 1) if len(ys) else 0

def pose(start):
    return re.sub(r"\d+$", "", start).rpartition("_")[0]

def target(p, h):
    """the height a pose's frame should have: its own unless it is off the pose's reference by more than TOL"""
    if p in REF and h and abs(h / REF[p] - 1) > TOL: return REF[p]
    return h

def _label(m):
    H, W = m.shape; lab = np.zeros((H, W), np.int32); n = 0
    for y0, x0 in zip(*np.nonzero(m)):
        if lab[y0, x0]: continue
        n += 1; lab[y0, x0] = n; st = [(y0, x0)]
        while st:
            y, x = st.pop()
            for yy, xx in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
                if 0 <= yy < H and 0 <= xx < W and m[yy, xx] and not lab[yy, xx]:
                    lab[yy, xx] = n; st.append((yy, xx))
    return lab, n

def clear_tail_hole(f):
    """stand_N's tail curl closed a light patch of the old background inside it (the owner, 2026-10-26): a light
    island on the dog's tail side whose edge is the dark outline becomes see-through, as the hole it was. The
    outline counts its grey-brown inner line too (lum < 110): PixMiniMax draws a curl lying on the floor that way,
    with the same light patch in its middle"""
    a = f[..., 3] > 0
    lum = f[..., 0] * 0.3 + f[..., 1] * 0.59 + f[..., 2] * 0.11
    dark = a & (lum < 110)
    xs = np.nonzero(a.any(0))[0]
    if not len(xs): return f
    xc = (xs.min() + xs.max()) / 2
    lab, n = _label(a & ~dark)
    out = f.copy()
    for k in range(1, n + 1):
        m = lab == k; area = m.sum()
        if not 3 <= area <= 80: continue
        g = m.copy(); g[1:] |= m[:-1]; g[:-1] |= m[1:]; g[:, 1:] |= m[:, :-1]; g[:, :-1] |= m[:, 1:]
        ring = g & ~m
        if dark[ring].mean() < 0.7 or lum[m].mean() < 150 or np.nonzero(m)[1].mean() <= xc: continue
        out[m] = 0
    return out

def clear_gap_blobs(f):
    """the same remnant where the gap between the tail and the body is open: PixMiniMax paints a grey-cream patch
    into the gap in a frame or two. Such a patch touches the transparent outside and hardly any of the fawn coat
    (the cream fur under the tail is bordered by the coat and the outline instead), so it is cleared"""
    a = f[..., 3] > 0
    rgb = f[..., :3].astype(int)
    lum = rgb[..., 0] * 0.3 + rgb[..., 1] * 0.59 + rgb[..., 2] * 0.11
    sat = rgb.max(2) - rgb.min(2)
    grey = a & (lum >= 150) & (sat < 60)
    coat = a & (lum >= 110) & (sat >= 60)
    xs = np.nonzero(a.any(0))[0]
    if not len(xs): return f
    xc = (xs.min() + xs.max()) / 2
    lab, n = _label(grey)
    out = f.copy()
    for k in range(1, n + 1):
        m = lab == k; area = m.sum()
        if not 4 <= area <= 60 or np.nonzero(m)[1].mean() <= xc: continue
        g = m.copy(); g[1:] |= m[:-1]; g[:-1] |= m[1:]; g[:, 1:] |= m[:, :-1]; g[:, :-1] |= m[:, 1:]
        ring = g & ~m
        if (~a[ring]).mean() >= 0.2 and coat[ring].mean() <= 0.3:
            out[m] = 0
    return out

def drop_specks(f, max_px=5):
    """8-connected pieces of a few pixels, apart from the dog: the generator's stray dots (the white specks under
    pee_SW's tail, what a cleared patch leaves around it)"""
    a = f[..., 3] > 0
    H, W = a.shape; seen = np.zeros_like(a); out = f.copy()
    for y0, x0 in zip(*np.nonzero(a)):
        if seen[y0, x0]: continue
        seen[y0, x0] = True; st = [(y0, x0)]; comp = []
        while st:
            y, x = st.pop(); comp.append((y, x))
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < H and 0 <= xx < W and a[yy, xx] and not seen[yy, xx]:
                        seen[yy, xx] = True; st.append((yy, xx))
        if len(comp) <= max_px:
            for y, x in comp: out[y, x] = 0
    return out

def scale(f, s):
    """the frame scaled by s about the pivot: shrinking takes the most common colour of the source pixels that
    fall in an output pixel (crisp, no new colours), opaque if more than half of them are; growing is nearest"""
    if abs(s - 1) < 1e-3: return f
    px, py = PIVOT
    if s > 1:
        o = (np.arange(CELL) + 0.5 - px) / s + px; sx = np.floor(o).astype(int)
        o = (np.arange(CELL) + 0.5 - py) / s + py; sy = np.floor(o).astype(int)
        ok = (sx[None, :] >= 0) & (sx[None, :] < CELL) & (sy[:, None] >= 0) & (sy[:, None] < CELL)
        out = f[np.clip(sy, 0, CELL - 1)[:, None], np.clip(sx, 0, CELL - 1)[None, :]].copy()
        out[~ok] = 0
        return out
    yy, xx = np.mgrid[0:CELL, 0:CELL]
    ox = np.floor((xx + 0.5 - px) * s + px).astype(int); oy = np.floor((yy + 0.5 - py) * s + py).astype(int)
    cell = (oy * CELL + ox).ravel()
    opaque = (f[..., 3] > 0).ravel()
    key = (f[..., 0].astype(np.int64) << 16 | f[..., 1].astype(np.int64) << 8 | f[..., 2]).ravel()
    total = np.bincount(cell, minlength=CELL * CELL)
    pairs, counts = np.unique(np.stack([cell[opaque], key[opaque]]), axis=1, return_counts=True)
    best = np.zeros(CELL * CELL, np.int64); bestn = np.zeros(CELL * CELL, np.int64)
    order = np.argsort(counts, kind="stable")                 # the largest count written last wins
    best[pairs[0, order]] = pairs[1, order]; bestn[pairs[0, order]] = counts[order]
    cov = np.bincount(cell[opaque], minlength=CELL * CELL)
    keep = cov * 2 > total
    out = np.zeros((CELL * CELL, 4), np.uint8)
    out[keep, 0] = best[keep] >> 16 & 255; out[keep, 1] = best[keep] >> 8 & 255; out[keep, 2] = best[keep] & 255
    out[keep, 3] = 255
    return out.reshape(CELL, CELL, 4)

def shift(f, dy):
    out = np.zeros_like(f)
    if dy >= 0: out[dy:] = f[:CELL - dy]
    else: out[:dy] = f[-dy:]
    return out

def ground_dy(f):
    ys = np.nonzero((f[..., 3] > 0).any(1))[0]
    return (PIVOT[1] - 1) - int(ys.max()) if len(ys) else 0

HOLES = None
def holes():
    """the tags made from stand_N (jobs.json) and the old walk_N: the remnant was copied into the first ones, and
    the remakes from the cleared stand_N still paint it back into the tail's curl or gap now and then"""
    global HOLES
    if HOLES is None:
        jobs = json.load(open(os.path.join(ROOT, "assets", "pixellab", "jobs.json"), encoding="utf-8"))["jobs"]
        HOLES = {j["tag"] for j in jobs if j["start"] == "stand_N"} | {"walk_N", "idle_stand_N"}
    return HOLES

def sheet_size(start, jobs, depth=0):
    """the scale a start frame needs for its Gemini sheet; a start frame PixelLab made (a transition's last frame,
    jobs.json "makes") has the scale of the start that animation grew from"""
    cells = json.load(open(CELLS, encoding="utf-8"))
    if start in cells: return SHEET_SIZE.get(cells[start]["cell"].split(".")[0], 1.0)
    for j in jobs:
        if j.get("makes") == start and depth < 4: return sheet_size(j["start"], jobs, depth + 1)
    return 1.0

def normalize(tag, jobs=None):
    """the sheet's frames (the source pose first) -> 128 px RGBA frames, one size, on the ground"""
    if jobs is None:
        jobs = json.load(open(os.path.join(ROOT, "assets", "pixellab", "jobs.json"), encoding="utf-8"))["jobs"]
    job = job_for(tag, jobs)
    name = split(tag)[0]
    frames = [to128(f) for f in sheet_frames(tag)]
    if split(tag)[0] + "_" + split(tag)[1] in holes() or tag in holes():
        frames = [clear_gap_blobs(clear_tail_hole(f)) for f in frames]
    frames = [drop_specks(f) for f in frames]
    p0 = pose(job["start"]); h0 = height(frames[0])
    s0 = target(p0, h0) / h0 if h0 else 1
    if p0 not in REF: s0 = sheet_size(job["start"], jobs)        # a bow, a carry, lying ...: its sheet's scale
    s1 = s0
    if name in END and END[name] in REF:
        hl = height(frames[-1]); s1 = target(END[name], hl) / hl if hl else s0
    n = len(frames)
    out = [scale(f, s0 + (s1 - s0) * (i / (n - 1) if n > 1 else 0)) for i, f in enumerate(frames)]
    if name in JUMP:
        dy = ground_dy(out[0]); return [shift(f, dy) for f in out]
    return [shift(f, ground_dy(f)) for f in out]

def fix_startframes():
    d = os.path.join(ROOT, "assets", "startframes")
    for fn in sorted(os.listdir(d)):
        if not fn.endswith(".png"): continue
        path = os.path.join(d, fn); f = np.asarray(Image.open(path).convert("RGBA"))
        g = clear_tail_hole(f) if fn == "stand_N.png" else f
        p = pose(fn[:-4]); h = height(g); s = target(p, h) / h if h else 1
        g = shift(scale(g, s), ground_dy(scale(g, s)))
        if not np.array_equal(g, f):
            Image.fromarray(g, "RGBA").save(path)
            print(f"  {fn:<16} {h} -> {height(g)} px" + ("  (tail hole cleared)" if fn == "stand_N.png" else ""))

def check():
    """every animation's first and last frame, by name and direction, on one red ground line, with the heights"""
    jobs = json.load(open(os.path.join(ROOT, "assets", "pixellab", "jobs.json"), encoding="utf-8"))["jobs"]
    tags = sorted(f[:-10] for f in os.listdir(os.path.join(ROOT, "assets", "pixellab"))
                  if f.endswith("_sheet.png") and not f.startswith("yard_"))     # the yard's parts are not Lili
    rows = {}
    for t in tags:
        name, d, var = split(t); rows.setdefault(name, []).append(t)
    S, T = 1, CELL
    names = sorted(rows)
    cols = max(len(v) for v in rows.values()) * 2
    sheet = Image.new("RGB", (cols * T + 120, len(names) * (T + 4)), (52, 55, 62))
    dr = ImageDraw.Draw(sheet)
    for r, name in enumerate(names):
        y = r * (T + 4); dr.text((4, y + 4), name, fill=(240, 240, 220))
        ts = sorted(rows[name], key=lambda t: (DIRS.index(split(t)[1]) if split(t)[1] in DIRS else 9, t))
        for k, t in enumerate(ts):
            fr = normalize(t, jobs)
            for e, f in enumerate((fr[0], fr[-1])):
                x = 120 + (2 * k + e) * T
                im = Image.fromarray(f, "RGBA"); sheet.paste(im, (x, y), im)
                dr.text((x + 2, y + 2), f"{t.replace(name + '_', '')}{' end' if e else ''} {height(f)}", fill=(200, 230, 255))
        dr.line([(120, y + PIVOT[1]), (sheet.width, y + PIVOT[1])], fill=(230, 70, 70))
    out = os.path.join(ROOT, "docs", "img", "anim", "size_check.png")
    sheet.save(out); print("->", out)

if __name__ == "__main__":
    if "--startframes" in sys.argv: fix_startframes()
    if "--check" in sys.argv: check()
