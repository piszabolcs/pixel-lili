#!/usr/bin/env python3
"""Pixel Lili: the approved PixelLab animations -> the game's sprite sets (2026-10-23, the 2D line; from the
normalized frames since 2026-10-27, M3.5).

    py -3.12 tools/pixellab_to_sprites.py

Each sheet goes through tools/pixellab_norm.py first: the 128 px cell, one size and one ground line for every
animation (the owner, 2026-10-26: nothing may jump or be 20-30 % bigger when they follow each other). The generated
frames only: frame 0 is the start pose, which differs a little (the ear lifts while walking) and would pop once
per loop. Then:
assets/sprites/lili_128/<anim>/d<dir>_f<i>.png  the full-size masters (indexed, Lili's 40 colours in slots
144-183, 0 = transparent), and from them the game's two sizes, which tools/pack_assets.py packs:
assets/sprites/lili/<anim>/ the room's (0.75, the owner picked it on 2026-10-23: at full size she looked too
cartoonish) and assets/sprites/lili_yard/<anim>/ the yard's (0.625, the yard is painted smaller), each with a
manifest.json. A set's cell is cropped to what its frames use in any direction (the 128 px cell is mostly air,
kept for the jumps), and its pivot moves with the crop, so the paws stay on the pivot row.
PixelLab kept Lili's palette (measured: 95 % of the pixels are exactly one of her 40 colours), so the mapping
is nearest-colour in Oklab; shrinking takes each output pixel's most common source colour and redraws the outline.
A direction PixelLab did not make falls back to the nearest one in the game (pak_frame).
"""
import json, os, sys
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from palette_from_photos import rgb_to_oklab
import pixellab_norm as pn
import sprite_clean as sc

DIRS = pn.DIRS
CELL, PIVOT, SLOT = pn.CELL, pn.PIVOT, 144

def all8(src):
    return {d: f"{src}_{d}" for d in DIRS}

# anim -> (catalogue id = the spec 7.4 A-number, fps, loop, {dir: sheet tag})
ANIMS = {
    # M3.5: spec 7.4 A01-A09, every direction (approved by the owner on 2026-10-26, trot_SE on 2026-10-27)
    "idle_stand":     (1,  6,  True,  all8("idle_stand")),
    "idle_sit_blink": (2,  6,  True,  all8("idle_sit_blink")),
    "sit_down":       (3,  12, False, all8("sit_down")),
    "walk":           (4,  10, True,  all8("walk")),
    "trot":           (5,  14, True,  all8("trot")),
    "run":            (6,  18, True,  all8("run")),
    "tiptoe":         (7,  8,  True,  all8("sneak")),          # A07: PixelLab's "sneaking slowly, crouched low"
    "scurry":         (8,  16, True,  all8("scurry")),
    "pant_smile":     (9,  8,  True,  all8("pant_smile")),
    # in the game since 2026-10-23, one direction each; M3.6 brings the rest and the approved remakes
    "play_bow":       (11, 10, True,  {"SW": "play_bow_SW_v3"}),   # v3 (the owner's prompt): all four paws planted
    "bark_head_flip": (12, 10, False, {"S": "bark_S"}),
    "eat_drink":      (17, 8,  True,  {"W": "eat_W"}),
    "sleep_curl":     (18, 3,  True,  {"SW": "sleep_curl_SW_v2"}),   # the owner rejected it on 2026-10-26 (sheet removed): the built set stays for now
    "look_around":    (23, 8,  False, {"S": "look_around_S"}),
    "drink":          (28, 8,  True,  {"W": "drink_W"}),
}
# played from another set backwards: no frames of their own in the pak (common/asset_ids.h ANIM_REVERSE_OF)
REVERSED = {"stand_up": (29, "sit_down")}     # A03's second half: sitting down backwards is standing up
# the game's sizes: (folder, scale, catalogue id offset); the yard's sets are anim id + 100 (pack_assets.py YARD_OFFSET)
SIZES = (("lili", 0.75, 0), ("lili_yard", 0.625, 100))

def palette():
    master = json.load(open(os.path.join(ROOT, "assets", "palettes", "master.json"), encoding="utf-8"))["rgb"]
    lili = json.load(open(os.path.join(ROOT, "assets", "palettes", "lili_sprites.json"), encoding="utf-8"))["rgb"]
    full = [list(c) for c in master]
    for i, c in enumerate(lili):
        full[SLOT + i] = list(c)
    return full, np.array(lili, float)

def to_index(rgba, lili_lab):
    a = rgba[..., 3] > 127
    idx = np.zeros(a.shape, np.uint8)
    lab = rgb_to_oklab(rgba[..., :3][a].reshape(-1, 3).astype(float))
    near = np.argmin(((lab[:, None, :] - lili_lab[None, :, :]) ** 2).sum(2), 1)
    idx[a] = SLOT + near
    return idx

def ground(idx, pivot_y, dy=None):
    """the lowest pixel on the row above the pivot (dy given: that shift instead, a jump keeps its height)"""
    if dy is None:
        ys = np.nonzero(idx.any(1))[0]
        if not len(ys): return idx
        dy = (pivot_y - 1) - ys.max()
    out = np.zeros_like(idx)
    if dy >= 0: out[dy:] = idx[:idx.shape[0] - dy]
    else: out[:dy] = idx[-dy:]
    return out

def outline_of(seqs):
    """the outline index: the commonest colour on the silhouettes' edge"""
    edge = []
    for s in seqs.values():
        for f in s:
            b = f != 0; p = np.pad(b, 1)
            edge.append(f[b & ~(p[:-2, 1:-1] & p[2:, 1:-1] & p[1:-1, :-2] & p[1:-1, 2:])])
    return int(np.bincount(np.concatenate(edge)).argmax())

def shrink(idx, full_rgb, pal_lab, outline, scale):
    """a 128 px master to a game size: each output pixel the most common of the source colours it covers
    (crisp, no new colours), the outline redrawn"""
    rgba = np.zeros(idx.shape + (4,), np.uint8)
    rgba[..., :3] = np.array(full_rgb, np.uint8)[idx]; rgba[..., 3] = np.where(idx > 0, 255, 0)
    small = sc.resample(rgba, 1 / scale, pal_lab, np.arange(SLOT, SLOT + 40))
    return sc.clean_outline(small, pal_lab, outline)

def crop(seqs, pivot):
    """every frame of every direction cut to their common bounding box; the pivot in the cut cell"""
    used = np.zeros_like(next(iter(seqs.values()))[0], bool)
    for s in seqs.values():
        for f in s: used |= f != 0
    ys, xs = np.nonzero(used)
    x0, y0, x1, y1 = xs.min(), ys.min(), max(xs.max() + 1, pivot[0] + 1), max(ys.max() + 1, pivot[1])
    return ({d: [f[y0:y1, x0:x1] for f in s] for d, s in seqs.items()}, (int(x1 - x0), int(y1 - y0)), (int(pivot[0] - x0), int(pivot[1] - y0)))

def convert(name, src, ctx, jobs=None):
    """one animation, {dir: tag} -> {"master": ({dir index: [128 px frames]}, ...), "lili": (seqs, (w, h), pivot), "lili_yard": ...}"""
    full, lili_lab, pal_lab = ctx
    seqs = {DIRS.index(d): [to_index(f, lili_lab) for f in pn.normalize(t, jobs)[1:]] for d, t in src.items()}
    n = len(next(iter(seqs.values())))
    assert all(len(s) == n for s in seqs.values()), f"{name}: the directions differ in frame count"
    out = {"master": (seqs, (CELL, CELL), PIVOT)}
    outline = outline_of(seqs)
    jump = pn.split(next(iter(src.values())))[0] in pn.JUMP
    for sub, scale, _ in SIZES:
        piv = (int(PIVOT[0] * scale), int((PIVOT[1] - 0.5) * scale) + 1)     # the ground row (121) lands on row piv_y - 1
        small = {}
        for d, s in seqs.items():
            fr = [shrink(f, full, pal_lab, outline, scale) for f in s]
            if jump:                                                        # the first frame on the ground, the rest keep their height
                dy = (piv[1] - 1) - int(np.nonzero(fr[0].any(1))[0].max())
                small[d] = [ground(f, piv[1], dy) for f in fr]
            else:
                small[d] = [ground(f, piv[1]) for f in fr]
        out[sub] = crop(small, piv)
    return out

def save(path, idx, full):
    im = Image.fromarray(idx, "P"); im.putpalette(sum(full, [])); im.info["transparency"] = 0
    im.save(path, transparency=0)

def write_set(d, anim, aid, fps, loop, seqs, size, pivot, full, src):
    if os.path.isdir(d):
        for f in os.listdir(d): os.remove(os.path.join(d, f))
    os.makedirs(d, exist_ok=True)
    files = []
    for di in sorted(seqs):
        for i, idx in enumerate(seqs[di]):
            fn = f"d{di}_f{i}.png"; save(os.path.join(d, fn), idx, full)
            files.append({"dir": di, "frame": i, "file": fn, "mouth": None, "head": None})
    man = {"anim": anim, "id": aid, "fps": fps, "loop": loop, "dirs": sorted(seqs), "frames": len(seqs[sorted(seqs)[0]]),
           "cell_w": size[0], "cell_h": size[1], "pivot": list(pivot), "files": files,
           "source": "tools/pixellab_to_sprites.py (" + ", ".join(src.values()) + ")"}
    json.dump(man, open(os.path.join(d, "manifest.json"), "w", encoding="utf-8"), indent=1)

def context():
    full, lili = palette()
    return full, rgb_to_oklab(lili), rgb_to_oklab(np.array(full, float))

def main():
    ctx = context(); full = ctx[0]
    jobs = json.load(open(os.path.join(ROOT, "assets", "pixellab", "jobs.json"), encoding="utf-8"))["jobs"]
    cat = {"_note": "The animation catalogue (spec 7.4): id = the A-number (drink 28 is new: the spec's A17 eat_drink was split, "
                    "PixelLab made eating and drinking apart; stand_up 29 is A03's second half, sit_down played backwards), fps, "
                    "dirs (the ones that exist, the rest fall back to the nearest), loop. The sprites come from tools/pixellab_to_sprites.py; "
                    "tools/pack_assets.py generates common/asset_ids.h from this file."}
    for anim, (aid, fps, loop, src) in ANIMS.items():
        gone = [t for t in src.values() if not os.path.exists(os.path.join(ROOT, "assets", "pixellab", t + "_sheet.png"))]
        if gone:                                             # the owner rejected the sheet: the built set stays until a new one is approved
            man = json.load(open(os.path.join(ROOT, "assets", "sprites", "lili", anim, "manifest.json"), encoding="utf-8"))
            cat[anim] = {"id": aid, "frames": man["frames"], "fps": fps, "dirs": [DIRS[d] for d in man["dirs"]], "loop": loop}
            print(f"{anim:15s} kept as built ({', '.join(gone)} was rejected, 2026-10-26)"); continue
        sets = convert(anim, src, ctx, jobs)
        seqs = sets["master"][0]
        write_set(os.path.join(ROOT, "assets", "sprites", "lili_128", anim), anim, aid, fps, loop, seqs, (CELL, CELL), PIVOT, full, src)
        line = []
        for sub, _, off in SIZES:
            s, size, piv = sets[sub]
            write_set(os.path.join(ROOT, "assets", "sprites", sub, anim), anim, aid + off, fps, loop, s, size, piv, full, src)
            line.append(f"{sub} {size[0]}x{size[1]} pivot {piv}")
        n = len(next(iter(seqs.values())))
        cat[anim] = {"id": aid, "frames": n, "fps": fps, "dirs": list(src), "loop": loop}
        print(f"{anim:15s} id {aid:2d}: {n} frames x {len(seqs)} dirs; " + ", ".join(line))
    for anim, (aid, base) in REVERSED.items():
        cat[anim] = dict(cat[base], id=aid, reverse_of=base)
    cat = {k: cat[k] for k in ["_note"] + sorted((k for k in cat if k != "_note"), key=lambda k: cat[k]["id"])}
    json.dump(cat, open(os.path.join(ROOT, "assets", "anims.json"), "w", encoding="utf-8"), indent=1)
    print("-> assets/sprites/lili_128/* (masters), lili/* (room), lili_yard/* (yard), assets/anims.json")

if __name__ == "__main__":
    main()
