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
import colorsys, json, os, sys
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from palette_from_photos import rgb_to_oklab
import pixellab_norm as pn
import sprite_clean as sc

DIRS = pn.DIRS
CELL, PIVOT, SLOT = pn.CELL, pn.PIVOT, 144
TOY_SLOT, TOY_N = 184, 6                   # the toys' colours (assets/palettes/toy_colors.json): the rope keeps its blues, reds, yellows
ROPE_ANIMS = ("tug_rope", "carry_trot", "lie_down_rope", "chew_toy")

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
    # M3.6 (2026-10-27): spec 7.4 A10-A27, every direction the review set has (docs/review: the owner's first round,
    # 2026-10-26, without its hidden sheets; the rest falls back to the nearest direction). Missing: A13 huff,
    # A14 pickup / drop, A21 guilty_peek, A22 jump_up / jump_down (no PixelLab sheets yet)
    "head_tilt":      (10, 8,  False, {d: f"head_tilt_{d}" for d in ("S", "SW", "SE")}),
    "play_bow":       (11, 10, True,  dict(all8("play_bow"), SW="play_bow_SW_v3")),   # v3 (the owner's prompt): all four paws planted
    "bark_head_flip": (12, 10, False, {"S": "bark_S_v2", "SW": "bark_SW", "W": "bark_W", "E": "bark_E", "SE": "bark_SE"}),   # no back views (the owner, 2026-10-26)
    "chew_toy":       (15, 10, True,  {"S": "chew_rope_S", "SE": "chew_rope_SE"}),
    "pet_enjoy":      (16, 6,  True,  {d: f"pet_enjoy_{d}" for d in ("S", "SW", "SE")}),
    "eat_drink":      (17, 8,  True,  {"W": "eat_W"}),
    "sleep_curl":     (18, 3,  True,  {"S": "sleep_curl_S"}),           # replaces the rejected SW (2026-10-26)
    "sun_sit":        (19, 4,  True,  {d: f"sun_sit_{d}" for d in ("S", "SW")}),
    "squat":          (20, 8,  False, {"SE": "squat_pee_SE"}),          # A20's way down; pee and poop (31, 32) hold the squat
    "look_around":    (23, 8,  False, {d: f"look_around_{d}" for d in ("S", "SW", "SE")}),
    "sniff":          (24, 8,  True,  all8("sniff")),
    "startle":        (25, 14, False, all8("startle")),
    "pounce":         (26, 14, False, all8("pounce")),
    "ask_pet":        (27, 8,  True,  {d: f"ask_pet_{d}" for d in ("S", "SW", "SE")}),
    "drink":          (28, 8,  True,  {"W": "drink_W_pmm"}),            # drink_W had a background remnant (the owner, 2026-10-26)
    # beyond the spec's list, made in the same PixelLab round and approved with it
    "lie_down":       (30, 8,  False, {d: f"lie_down_{d}" for d in ("S", "SW", "SE")}),
    "pee":            (31, 6,  True,  {d: f"pee_{d}" for d in ("SW", "SE")}),
    "poop":           (32, 6,  True,  {d: f"poop_{d}" for d in ("SW", "SE")}),
    "tug_rope":       (33, 10, True,  {d: f"tug_rope_{d}" for d in ("S", "SW", "SE")}),
    "eat_treat":      (34, 8,  True,  {d: f"eat_treat_{d}" for d in ("S", "SW", "SE")}),
    "carry_trot":     (35, 14, True,  {d: f"carry_trot_{d}" for d in ("S", "SW", "W", "NW", "NE", "E", "SE")}),
    "lie_down_rope":  (36, 8,  False, {"S": "lie_down_rope_S"}),
    "chew_treat":     (37, 10, True,  {d: f"chew_treat_{d}" for d in ("S", "SE")}),
}
# played from another set backwards: no frames of their own in the pak (common/asset_ids.h ANIM_REVERSE_OF)
REVERSED = {"stand_up": (29, "sit_down"),    # A03's second half: sitting down backwards is standing up
            "get_up": (38, "lie_down")}      # 2026-10-28: lying down backwards is getting up (common/lili/pose.c)
# how a set plays where it is not once or round and round (pack_assets.py: the pak's loop byte 2 / 3). 2026-10-28, the owner:
# "choppy, not matched": PixMiniMax makes a motion from its first frame, not a cycle, so these loops jumped back from their
# last frame to their first (the jump was the cycle's biggest step in at least one direction); there and back has no such
# jump. The startle ends in the air: there and back once, she lands.
PLAY = {"idle_stand": "pingpong", "idle_sit_blink": "pingpong", "pant_smile": "pingpong", "sniff": "pingpong",
        "ask_pet": "pingpong", "eat_drink": "pingpong", "startle": "there_and_back"}
# the game's sizes: (folder, scale, catalogue id offset); the yard's sets are anim id + 100 (pack_assets.py YARD_OFFSET)
SIZES = (("lili", 0.75, 0), ("lili_yard", 0.625, 100))

def palette():
    master = json.load(open(os.path.join(ROOT, "assets", "palettes", "master.json"), encoding="utf-8"))["rgb"]
    lili = json.load(open(os.path.join(ROOT, "assets", "palettes", "lili_sprites.json"), encoding="utf-8"))["rgb"]
    toy = json.load(open(os.path.join(ROOT, "assets", "palettes", "toy_colors.json"), encoding="utf-8"))["rgb"]
    full = [list(c) for c in master]
    for i, c in enumerate(lili):
        full[SLOT + i] = list(c)
    for i, c in enumerate(toy):
        full[TOY_SLOT + i] = list(c)
    return full, np.array(lili, float)

def rope_like(rgb):
    """a pixel of the rope toy, not of Lili: its blues, its saturated reds, its yellows (her fawn stays under 42 degrees,
    her pink ears and tongue are paler than the rope's red)"""
    r, g, b = [c / 255 for c in rgb]
    h, sat, v = colorsys.rgb_to_hsv(r, g, b); h *= 360
    if v < 0.3: return False
    if 180 <= h <= 270 and sat >= 0.30: return True
    if (h >= 335 or h <= 12) and sat >= 0.50: return True
    return 43 <= h <= 65 and sat >= 0.45 and v >= 0.70

def to_index(rgba, lili_lab, toy=False):
    """nearest of Lili's 40 colours in Oklab; with toy=True the rope's pixels may also take the toys' 6"""
    a = rgba[..., 3] > 127
    idx = np.zeros(a.shape, np.uint8)
    px = rgba[..., :3][a].reshape(-1, 3).astype(float)
    lab = rgb_to_oklab(px)
    near = SLOT + np.argmin(((lab[:, None, :] - lili_lab[None, :, :]) ** 2).sum(2), 1)
    if toy:
        toy_lab = rgb_to_oklab(np.array(palette()[0][TOY_SLOT:TOY_SLOT + TOY_N], float))
        rope = np.array([rope_like(c) for c in px])
        if rope.any():
            near[rope] = TOY_SLOT + np.argmin(((lab[rope][:, None, :] - toy_lab[None]) ** 2).sum(2), 1)
    idx[a] = near
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
    small = sc.resample(rgba, 1 / scale, pal_lab, np.r_[np.arange(SLOT, SLOT + 40), np.arange(TOY_SLOT, TOY_SLOT + TOY_N)])
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
    seqs = {DIRS.index(d): [to_index(f, lili_lab, name in ROPE_ANIMS) for f in pn.normalize(t, jobs)[1:]] for d, t in src.items()}
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
    only = set(sys.argv[1:])                                 # some animations only; the rest keep their built sets
    old = json.load(open(os.path.join(ROOT, "assets", "anims.json"), encoding="utf-8"))
    cat = {"_note": "The animation catalogue (spec 7.4): id = the A-number (drink 28 is new: the spec's A17 eat_drink was split, "
                    "PixelLab made eating and drinking apart; stand_up 29 is A03's second half, sit_down played backwards), fps, "
                    "dirs (the ones that exist, the rest fall back to the nearest), loop. The sprites come from tools/pixellab_to_sprites.py; "
                    "tools/pack_assets.py generates common/asset_ids.h from this file."}
    for k, v in old.items():                                 # the toys and other sets made elsewhere (tools/toy_sprites.py)
        if isinstance(v, dict) and v.get("extra"): cat[k] = v
    for anim, (aid, fps, loop, src) in ANIMS.items():
        if only and anim not in only:
            cat[anim] = old[anim]; continue
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
    for anim, play in PLAY.items():
        if anim in cat: cat[anim]["play"] = play
    for anim, (aid, base) in REVERSED.items():
        cat[anim] = {k: v for k, v in dict(cat[base], id=aid, reverse_of=base).items() if k != "play"}
    cat = {k: cat[k] for k in ["_note"] + sorted((k for k in cat if k != "_note"), key=lambda k: cat[k]["id"])}
    json.dump(cat, open(os.path.join(ROOT, "assets", "anims.json"), "w", encoding="utf-8"), indent=1)
    print("-> assets/sprites/lili_128/* (masters), lili/* (room), lili_yard/* (yard), assets/anims.json")

if __name__ == "__main__":
    main()
