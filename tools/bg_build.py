#!/usr/bin/env python3
"""Pixel Lili: the prepared backgrounds (tools/bg_prepare.py, 400 grid) -> game layers for the pak (2026-10-23).

    py -3.12 tools/bg_build.py

-> assets/backgrounds/game/*.png (indexed, 480 x 480 space) + game.json, which tools/pack_assets.py packs:
  room, yard_<season>          the static pictures; the room's 9 glass panes are index 0 (the door view shows)
  door_<season>                what the panes show, placed at the window
  pad, vase, bowl_food, ...    the room's props (they sort with Lili; the pad is flat)
  mask_sun, mask_cast, mask_shade_out, mask_shade_in
                               where the afternoon sunbeam lands, where the door's light leaves the bench's, the
                               bowls' and the vase's cast shadows, and the overhead light's contact shadows
  palettes                     per background: the master palette (fixed 0-15, Lili 144-183) with the 200 free
                               slots (16-143, 184-255) from k-means in Oklab over that picture; the training set
                               also holds the picture's floor/grass darkened and lightened, so a shadow or a sunbeam
                               has a real colour to step to
  luts                         per palette, index -> index: sun_strong, sun_weak, cast_day, cast_eve, shade_out,
                               shade_in (the light and shadow at run time are these remaps, spec 7.2)
Scaling 400 -> 480 is nearest (every 5th pixel doubled); every layer is placed on a full 400 canvas first and
scaled whole, so the layers stay pixel-aligned with each other.
"""
import json, os, sys
import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from palette_from_photos import rgb_to_oklab
import bg_prepare as bp

BG = os.path.join(ROOT, "assets", "backgrounds")
OUT = os.path.join(BG, "game")
SHOW, GRID = 480, 400
SLOTS = list(range(16, 144)) + list(range(184, 256))           # 200 slots a background may use
TABLES = {  # name: (multiply, add) on RGB, the same numbers the preview (bg_prepare) used
    "sun_strong": (1.16, (30, 20, 2)), "sun_weak": (1.056, (10.5, 7, 0.7)),
    "cast_day": (0.64, (0, 1.44, 5.04)), "cast_eve": (0.838, (0, 0.65, 2.27)),
    "shade_out": (0.80, (0, 0.8, 2.8)), "shade_in": (0.62, (0, 1.52, 5.32))}
SEASONS = bp.SEASONS
# the yard's covers (2026-10-23, the owner: she must go behind and into the bushes, behind the trees and the
# bench): hand-traced on the 400 grid (the four seasons share the layout), each with the floor row it stands on
# (its Y-sort key). Cut from each season's picture, drawn over Lili when she is behind that row.
COVERS = [
    ("top_bush",   [(104, 62), (118, 50), (142, 48), (156, 58), (160, 78), (156, 96), (144, 105), (116, 105), (104, 96), (100, 78)], 104),
    ("left_tree",  [(0, 72), (30, 70), (60, 74), (95, 90), (112, 112), (120, 140), (118, 160), (108, 178), (90, 190), (55, 196), (52, 212),
                    (50, 240), (55, 262), (60, 276), (40, 280), (15, 282), (20, 262), (22, 230), (20, 196), (0, 192)], 278),
    ("right_tree", [(242, 40), (250, 20), (275, 6), (310, 0), (399, 0), (399, 105), (380, 112), (365, 118), (362, 135), (368, 150),
                    (355, 156), (330, 158), (312, 150), (320, 135), (318, 118), (300, 112), (270, 108), (245, 96), (240, 70)], 156),
    ("right_bush", [(340, 170), (352, 156), (372, 152), (392, 158), (399, 170), (399, 200), (390, 208), (360, 210), (344, 202), (336, 186)], 208),
    ("bench",      [(262, 262), (280, 240), (300, 238), (316, 252), (372, 288), (374, 344), (366, 346), (360, 330), (336, 322), (336, 344),
                    (328, 346), (326, 326), (290, 306), (286, 312), (262, 306)], 330),
    ("shrubs",     [(0, 290), (20, 282), (45, 286), (70, 300), (88, 320), (92, 345), (90, 370), (80, 399), (0, 399)], 399),
]

def up(img400):
    return img400.resize((SHOW, SHOW), Image.NEAREST)

def layer(rgba_small, pos):
    """an RGBA piece placed on a transparent 400 canvas, scaled, cropped to its pixels: (rgba, x, y)"""
    c = Image.new("RGBA", (GRID, GRID), (0, 0, 0, 0)); c.paste(rgba_small, pos)
    big = np.asarray(up(c))
    ys, xs = np.nonzero(big[..., 3])
    x0, y0, x1, y1 = xs.min(), ys.min(), xs.max() + 1, ys.max() + 1
    return big[y0:y1, x0:x1], int(x0), int(y0)

def mask_layer(m400):
    m = np.asarray(up(Image.fromarray(m400.astype(np.uint8) * 255))) > 0
    ys, xs = np.nonzero(m)
    x0, y0, x1, y1 = xs.min(), ys.min(), xs.max() + 1, ys.max() + 1
    return m[y0:y1, x0:x1], int(x0), int(y0)

def kmeans(lab, k, iters=14, seed=1):
    rng = np.random.default_rng(seed)
    uniq = np.unique(np.round(lab, 3), axis=0)
    c = uniq[rng.choice(len(uniq), size=min(k, len(uniq)), replace=False)]
    for _ in range(iters):
        lab_l = []
        a = np.empty(len(lab), np.int32)
        for s in range(0, len(lab), 65536):
            a[s:s + 65536] = np.argmin(((lab[s:s + 65536, None, :] - c[None]) ** 2).sum(2), 1)
        for j in range(len(c)):
            sel = lab[a == j]
            if len(sel): c[j] = sel.mean(0)
            else: c[j] = lab[rng.integers(len(lab))]
    return c

def oklab_to_rgb(lab):
    L, a, b = lab[:, 0], lab[:, 1], lab[:, 2]
    l_ = L + 0.3963377774 * a + 0.2158037573 * b; m_ = L - 0.1055613458 * a - 0.0638541728 * b; s_ = L - 0.0894841775 * a - 1.2914855480 * b
    l, m, s = l_ ** 3, m_ ** 3, s_ ** 3
    r = 4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s
    g = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s
    bb = -0.0041960863 * l - 0.7034186147 * m + 1.7076086130 * s
    lin = np.stack([r, g, bb], 1).clip(0, 1)
    srgb = np.where(lin <= 0.0031308, 12.92 * lin, 1.055 * np.power(lin, 1 / 2.4) - 0.055)
    return (srgb * 255).round().clip(0, 255).astype(int)

def build_palette(pixels, floor_pixels, master):
    """200 colours for one background; the floor (or grass) also enters darkened and lightened"""
    train = [pixels]
    for name, (m, add) in TABLES.items():
        train.append((floor_pixels * m + np.array(add)).clip(0, 255))
    rgb = np.concatenate(train).astype(float)
    rng = np.random.default_rng(3)
    if len(rgb) > 120000: rgb = rgb[rng.choice(len(rgb), 120000, replace=False)]
    centres = oklab_to_rgb(kmeans(rgb_to_oklab(rgb), len(SLOTS)))
    full = [list(c) for c in master]
    for s, c in zip(SLOTS, centres): full[s] = [int(v) for v in c]
    return full

def index(rgb_pixels, full):
    pal = np.array([full[s] for s in SLOTS], float)
    lab = rgb_to_oklab(pal); px = rgb_to_oklab(rgb_pixels.reshape(-1, 3).astype(float))
    out = np.empty(len(px), np.int32)
    for s in range(0, len(px), 65536):
        out[s:s + 65536] = np.argmin(((px[s:s + 65536, None, :] - lab[None]) ** 2).sum(2), 1)
    return np.array(SLOTS, np.uint8)[out].reshape(rgb_pixels.shape[:-1])

def luts(full):
    pal = np.array([full[s] for s in SLOTS], float); lab = rgb_to_oklab(pal)
    out = {}
    for name, (m, add) in TABLES.items():
        t = list(range(256))
        tgt = rgb_to_oklab((pal * m + np.array(add)).clip(0, 255))
        near = np.argmin(((tgt[:, None, :] - lab[None]) ** 2).sum(2), 1)
        for i, s in enumerate(SLOTS): t[s] = SLOTS[near[i]]
        out[name] = t
    return out

def save_indexed(path, idx, full):
    im = Image.fromarray(idx.astype(np.uint8), "P"); im.putpalette(sum(full, [])); im.save(path, transparency=0)

def main():
    os.makedirs(OUT, exist_ok=True)
    master = json.load(open(os.path.join(ROOT, "assets", "palettes", "master.json"), encoding="utf-8"))["rgb"]
    lili = json.load(open(os.path.join(ROOT, "assets", "palettes", "lili_sprites.json"), encoding="utf-8"))["rgb"]
    master = [list(c) for c in master]
    for i, c in enumerate(lili): master[144 + i] = list(c)
    meta = {"palettes": {}, "luts": {}, "images": []}

    # ---- the room: picture, door views, props, masks, one palette for all of them ----
    room400 = Image.open(os.path.join(BG, "room.png")).convert("RGBA")
    room = np.asarray(up(room400))
    doors = {}
    for s in SEASONS:
        dv = Image.open(os.path.join(BG, f"door_view_{s}.png")).convert("RGBA")
        doors[s] = layer(dv, bp.WINDOW[:2])
    props = {}
    for pid, box in bp.PROPS.items():
        sp = Image.open(os.path.join(BG, "props", f"{pid}.png")).convert("RGBA")
        props[pid] = layer(sp, box[:2])
    floor = room[int(bp.FLOOR_TOP * 1.2):int(bp.FLOOR_BOTTOM * 1.2), :, :3].reshape(-1, 3)
    pix = [room[room[..., 3] > 0][:, :3]]
    for s in SEASONS: pix += [doors[s][0][..., :3].reshape(-1, 3)] * 20      # small but must keep their greens
    for pid in props: pix += [props[pid][0][props[pid][0][..., 3] > 0][:, :3]] * 3
    full = build_palette(np.concatenate(pix), floor, master)
    meta["palettes"]["room"] = full; meta["luts"]["room"] = luts(full)
    idx = index(room[..., :3], full); idx[room[..., 3] == 0] = 0
    save_indexed(os.path.join(OUT, "room.png"), idx, full)
    meta["images"].append({"name": "room", "file": "room.png", "x": 0, "y": 0, "palette": "room"})
    for s in SEASONS:
        rgba, x, y = doors[s]
        save_indexed(os.path.join(OUT, f"door_{s}.png"), index(rgba[..., :3], full), full)
        meta["images"].append({"name": f"door_{s}", "file": f"door_{s}.png", "x": x, "y": y, "palette": "room"})
    for pid, (rgba, x, y) in props.items():
        i2 = index(rgba[..., :3], full); i2[rgba[..., 3] == 0] = 0
        save_indexed(os.path.join(OUT, f"{pid}.png"), i2, full)
        ys = np.nonzero(rgba[..., 3])[0]
        meta["images"].append({"name": pid, "file": f"{pid}.png", "x": x, "y": y, "palette": "room", "sort_y": int(y + ys.max())})
    # the masks, computed on the 400 grid exactly as the preview did, then scaled
    grey = Image.new("RGB", (GRID, GRID), (100, 100, 100))
    sun = np.asarray(bp.sunbeam(grey, 1.0)).sum(2) != 300
    cast = np.zeros((GRID, GRID), bool)
    for bm, base, hmax in bp.bench_alpha():
        cast |= bp.cast_mask(bm, 0, 0, base, hmax, bp.LIGHT, (GRID, GRID))
    yy, xx = np.mgrid[0:GRID, 0:GRID]
    sh_out = np.zeros((GRID, GRID), bool); sh_in = np.zeros((GRID, GRID), bool)
    for pid in ("bowl_food", "bowl_water", "vase"):
        sp = Image.open(os.path.join(BG, "props", f"{pid}.png")).convert("RGBA")
        px, py = bp.PROPS[pid][:2]
        al = np.asarray(sp)[..., 3] > 0
        cast |= bp.cast_mask(al, px, py, py + np.nonzero(al)[0].max(), 80, bp.LIGHT, (GRID, GRID))
        cx, cy, rx, ry = bp.prop_shadow(pid, sp, px, py)
        d = ((xx - cx) / rx) ** 2 + ((yy - cy) / ry) ** 2
        sh_out |= d <= 1.0; sh_in |= d <= 0.55
    cast = bp.floor_only(cast)
    for name, m in (("mask_sun", sun), ("mask_cast", cast), ("mask_shade_out", sh_out & ~sh_in), ("mask_shade_in", sh_in)):
        mm, x, y = mask_layer(m)
        Image.fromarray(mm.astype(np.uint8) * 255).save(os.path.join(OUT, name + ".png"))
        meta["images"].append({"name": name, "file": name + ".png", "x": x, "y": y, "mask": True})

    # ---- the yards: one picture and one palette per season ----
    for s in SEASONS:
        yard = np.asarray(up(Image.open(os.path.join(BG, f"yard_{s}.png")).convert("RGB")))
        full = build_palette(yard.reshape(-1, 3), yard[160:440].reshape(-1, 3), master)
        meta["palettes"][f"yard_{s}"] = full; meta["luts"][f"yard_{s}"] = luts(full)
        yidx = index(yard, full)
        save_indexed(os.path.join(OUT, f"yard_{s}.png"), yidx, full)
        meta["images"].append({"name": f"yard_{s}", "file": f"yard_{s}.png", "x": 0, "y": 0, "palette": f"yard_{s}"})
        for k, (cname, poly, base) in enumerate(COVERS):
            m = Image.new("L", (GRID, GRID), 0); ImageDraw.Draw(m).polygon(poly, fill=255)
            mm, x, y = mask_layer(np.asarray(m) > 0)
            piece = np.where(mm, yidx[y:y + mm.shape[0], x:x + mm.shape[1]], 0).astype(np.uint8)
            save_indexed(os.path.join(OUT, f"cover_{s}_{cname}.png"), piece, full)
            meta["images"].append({"name": f"cover_{s}_{k}", "file": f"cover_{s}_{cname}.png", "x": x, "y": y,
                                   "palette": f"yard_{s}", "sort_y": int(round(base * SHOW / GRID))})

    json.dump(meta, open(os.path.join(OUT, "game.json"), "w", encoding="utf-8"))
    for name, t in meta["luts"]["room"].items():                   # does each table really step away from itself?
        moved = sum(1 for s in SLOTS if t[s] != s)
        print(f"room lut {name:10s}: {moved}/{len(SLOTS)} slots step to another colour")
    print(f"-> {os.path.relpath(OUT, ROOT)}: {len(meta['images'])} images, {len(meta['palettes'])} palettes")

if __name__ == "__main__":
    main()
