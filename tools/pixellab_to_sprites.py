#!/usr/bin/env python3
"""Pixel Lili: the approved PixelLab animations -> the game's sprite sets (2026-10-23, the 2D line).

    py -3.12 tools/pixellab_to_sprites.py

assets/pixellab/<tag>_sheet.png (one row of 96 px frames: the source pose twice, then the generated ones) ->
assets/sprites/lili/<anim>/d<dir>_f<i>.png (indexed, Lili's 40 colours in slots 144-183, 0 = transparent)
+ manifest.json, which tools/pack_assets.py packs. The generated frames only: the source pose differs a
little (the ear lifts while walking), so it would pop once per loop. PixelLab kept Lili's palette (measured:
95 % of the pixels are exactly one of her 40 colours), so the mapping is nearest-colour in Oklab.
The directions PixelLab did not make yet fall back to the nearest one in the game (pak_frame).
idle_stand: SW is PixelLab's tail wag; the other five directions keep the breathing from tools/animate2d.py,
stretched to the same 8 frames.
"""
import json, os, sys
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from palette_from_photos import rgb_to_oklab
import sprite_clean as sc

DIRS = ("S", "SW", "W", "NW", "N", "NE", "E", "SE")
CELL, PIVOT, SLOT = 96, (48, 90), 144
# anim -> (catalogue id, fps, loop, {dir: sheet tag})
ANIMS = {
    "idle_stand":     (1,  6,  True,  {"SW": "idle_stand_SW"}),
    "idle_sit_blink": (2,  6,  True,  {"S": "idle_sit_blink_S"}),
    "walk":           (4,  10, True,  {"S": "walk_S", "SW": "walk_SW", "W": "walk_W", "N": "walk_N", "NE": "walk_NE", "SE": "walk_SE"}),
    "trot":           (5,  12, True,  {"SE": "run_SE"}),
    "pant_smile":     (9,  8,  True,  {"S": "pant_smile_S"}),
    "play_bow":       (11, 10, True,  {"SW": "play_bow_SW_v3"}),   # v3 (2026-10-23, the owner's prompt): all four paws planted; v1 batted its paws, v2 lifted a hind leg
    "bark_head_flip": (12, 10, False, {"S": "bark_S"}),
    "eat_drink":      (17, 8,  True,  {"W": "eat_W"}),
    "sleep_curl":     (18, 3,  True,  {"SW": "sleep_curl_SW_v2"}),
    "look_around":    (23, 8,  False, {"S": "look_around_S"}),
    "drink":          (28, 8,  True,  {"W": "drink_W"}),
}
BREATH = (0, 0, 1, 1, 1, 1, 0, 0)        # idle_stand's other directions: the old 6-frame breath stretched to 8
# outdoors she is smaller: the yard paintings are drawn at a smaller scale than the room (the owner: half the
# bench, a third of the tree is too big); 0.625 makes a 60 px cell, pivot (30, 56). Anim id + YARD_OFFSET.
YARD_SCALE, YARD_CELL, YARD_PIVOT, YARD_OFFSET = 0.625, 60, (30, 56), 100

def grounded(idx, pivot_y):
    """the lowest paw on the row above the pivot: PixelLab's frames sit 1-6 px higher in some walk and trot
    frames, which made her float over her own shadow (the owner, 2026-10-23); in a walk some paw is always
    down, so the lowest opaque row is the ground"""
    ys = np.nonzero(idx.any(1))[0]
    if not len(ys): return idx
    dy = (pivot_y - 1) - ys.max()
    out = np.zeros_like(idx)
    if dy >= 0: out[dy:] = idx[:idx.shape[0] - dy]
    else: out[:dy] = idx[-dy:]
    return out

def yard_frame(idx, full_rgb, pal_lab, outline):
    """the 96 px frame down to the yard's 60 px: each output pixel the most common of the source colours it
    covers (crisp, no new colours), the outline redrawn, grounded again"""
    rgba = np.zeros(idx.shape + (4,), np.uint8)
    rgba[..., :3] = np.array(full_rgb, np.uint8)[idx]; rgba[..., 3] = np.where(idx > 0, 255, 0)
    small = sc.resample(rgba, 1 / YARD_SCALE, pal_lab, np.arange(SLOT, SLOT + 40))
    small = sc.clean_outline(small, pal_lab, outline)
    cell = np.zeros((YARD_CELL, YARD_CELL), np.uint8)
    h, w = min(YARD_CELL, small.shape[0]), min(YARD_CELL, small.shape[1])
    cell[:h, :w] = small[:h, :w]
    return grounded(cell, YARD_PIVOT[1])

def palette():
    master = json.load(open(os.path.join(ROOT, "assets", "palettes", "master.json"), encoding="utf-8"))["rgb"]
    lili = json.load(open(os.path.join(ROOT, "assets", "palettes", "lili_sprites.json"), encoding="utf-8"))["rgb"]
    full = [list(c) for c in master]
    for i, c in enumerate(lili):
        full[SLOT + i] = list(c)
    return full, np.array(lili, float)

def frames(tag):
    sheet = np.asarray(Image.open(os.path.join(ROOT, "assets", "pixellab", tag + "_sheet.png")).convert("RGBA"))
    cells = [sheet[:, i * CELL:(i + 1) * CELL] for i in range(sheet.shape[1] // CELL)]
    return [c for c in cells[1:] if not np.array_equal(c, cells[0])]

def to_index(rgba, lili_lab):
    a = rgba[..., 3] > 127
    idx = np.zeros(a.shape, np.uint8)
    lab = rgb_to_oklab(rgba[..., :3][a].reshape(-1, 3).astype(float))
    near = np.argmin(((lab[:, None, :] - lili_lab[None, :, :]) ** 2).sum(2), 1)
    idx[a] = SLOT + near
    return idx

def save(path, idx, full):
    im = Image.fromarray(idx, "P"); im.putpalette(sum(full, [])); im.info["transparency"] = 0
    im.save(path, transparency=0)

def write_set(d, anim, aid, fps, loop, seqs, n, cell, pivot, full, src):
    if os.path.isdir(d):
        for f in os.listdir(d): os.remove(os.path.join(d, f))
    os.makedirs(d, exist_ok=True)
    files = []
    for di in sorted(seqs):
        for i, idx in enumerate(seqs[di]):
            fn = f"d{di}_f{i}.png"; save(os.path.join(d, fn), idx, full)
            files.append({"dir": di, "frame": i, "file": fn, "mouth": None, "head": None})
    man = {"anim": anim, "id": aid, "fps": fps, "loop": loop, "dirs": sorted(seqs), "frames": n, "cell": cell,
           "scale": 1, "pivot": list(pivot), "files": files, "source": "tools/pixellab_to_sprites.py (" + ", ".join(src.values()) + ")"}
    json.dump(man, open(os.path.join(d, "manifest.json"), "w", encoding="utf-8"), indent=1)

def main():
    full, lili = palette()
    lili_lab = rgb_to_oklab(lili)
    pal_lab = rgb_to_oklab(np.array(full, float))
    for anim, (aid, fps, loop, src) in ANIMS.items():
        d = os.path.join(ROOT, "assets", "sprites", "lili", anim)
        old_breath = {}
        if anim == "idle_stand" and os.path.exists(os.path.join(d, "manifest.json")):
            man = json.load(open(os.path.join(d, "manifest.json"), encoding="utf-8"))
            for f in man["files"]:
                if DIRS[f["dir"]] not in src:
                    old_breath.setdefault(f["dir"], {})[f["frame"]] = np.asarray(Image.open(os.path.join(d, f["file"])))
        seqs = {DIRS.index(k): [to_index(fr, lili_lab) for fr in frames(tag)] for k, tag in src.items()}
        n = len(next(iter(seqs.values())))
        assert all(len(s) == n for s in seqs.values()), anim
        for di, fr in old_breath.items():                    # 0 and 2 are the breath's low and high frames
            seqs[di] = [fr[0] if b == 0 else fr[2] for b in BREATH]
        seqs = {di: [grounded(f, PIVOT[1]) for f in s] for di, s in seqs.items()}
        write_set(d, anim, aid, fps, loop, seqs, n, CELL, PIVOT, full, src)
        edge = []                                            # the outline index: the commonest on the silhouettes' edge
        for s in seqs.values():
            for f in s:
                b = f != 0; p = np.pad(b, 1)
                edge.append(f[b & ~(p[:-2, 1:-1] & p[2:, 1:-1] & p[1:-1, :-2] & p[1:-1, 2:])])
        outline = int(np.bincount(np.concatenate(edge)).argmax())
        yard = {di: [yard_frame(f, full, pal_lab, outline) for f in s] for di, s in seqs.items()}
        write_set(os.path.join(ROOT, "assets", "sprites", "lili_yard", anim), anim, aid + YARD_OFFSET, fps, loop, yard, n,
                  YARD_CELL, YARD_PIVOT, full, src)
        print(f"{anim:15s} id {aid:2d}: {n} frames x dirs {[DIRS[k] for k in sorted(seqs)]} (+ the yard's {YARD_CELL} px set, id {aid + YARD_OFFSET})")
    cat = {"_note": "The animation catalogue (spec 7.4): id = the A-number (drink 28 is new: the spec's A17 eat_drink was split, "
                    "PixelLab made eating and drinking apart), fps, dirs (the ones that exist, the rest fall back to the nearest), loop. "
                    "The sprites come from tools/pixellab_to_sprites.py (2026-10-23); tools/pack_assets.py generates common/asset_ids.h from this file."}
    for anim, (aid, fps, loop, src) in sorted(ANIMS.items(), key=lambda kv: kv[1][0]):
        dirs = list(src) if anim != "idle_stand" else ["S", "SW", "W", "N", "NE", "SE"]
        cat[anim] = {"id": aid, "frames": 4 if anim == "sleep_curl" else 8, "fps": fps, "dirs": dirs, "loop": loop}
    json.dump(cat, open(os.path.join(ROOT, "assets", "anims.json"), "w", encoding="utf-8"), indent=1)
    print("-> assets/sprites/lili/*, assets/anims.json")

if __name__ == "__main__":
    main()
