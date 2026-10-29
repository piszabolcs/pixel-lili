#!/usr/bin/env python3
"""Pixel Lili: the bowls by what is left in them (2026-10-29, the owner: "I do not get when to feed or water her").

    py -3.12 tools/bowl_levels.py          after tools/bg_build.py; then tools/pack_assets.py

assets/backgrounds/game/bowl_food.png / bowl_water.png (the room's props, indexed in the room's palette) are full;
this writes bowl_food_2 / _1 / _0 and bowl_water_3 / _2 / _1 / _0 beside them (the room's water bowl showed no water): the kibble or the water cut back to a
smaller, lower patch inside the rim, the rest painted as the bowl's own inside (the food bowl's white ceramic,
shaded at the back; the water bowl's dry blue, the water lighter with a glint), back in the room's palette.
pack_assets.py packs them (IMG_BOWL_FOOD_2 ...) at the full bowl's place; lili_world.c shows the one that fits.
"""
import colorsys, json, os
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
G = os.path.join(ROOT, "assets", "backgrounds", "game")

def ellipse(shape, cy, cx, ry, rx):
    yy, xx = np.mgrid[0:shape[0], 0:shape[1]]
    return ((yy - cy) / ry) ** 2 + ((xx - cx) / rx) ** 2 <= 1.0

def biggest(m):
    seen = np.zeros(m.shape, bool); best = []
    for y0, x0 in zip(*np.nonzero(m)):
        if seen[y0, x0]: continue
        part, st = [], [(y0, x0)]; seen[y0, x0] = True
        while st:
            y, x = st.pop(); part.append((y, x))
            for yy, xx in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
                if 0 <= yy < m.shape[0] and 0 <= xx < m.shape[1] and m[yy, xx] and not seen[yy, xx]: seen[yy, xx] = True; st.append((yy, xx))
        if len(part) > len(best): best = part
    o = np.zeros(m.shape, bool)
    for y, x in best: o[y, x] = True
    return o

def rows_filled(m):
    o = m.copy()
    for y in range(m.shape[0]):
        xs = np.nonzero(m[y])[0]
        if len(xs): o[y, xs.min():xs.max() + 1] = True
    return o

def main():
    pal = np.array(json.load(open(os.path.join(G, "game.json"), encoding="utf-8"))["palettes"]["room"], float)
    for name in ("bowl_food", "bowl_water"):
        idx = np.asarray(Image.open(os.path.join(G, name + ".png")))
        a = pal[idx]; opaque = idx > 0
        used = np.unique(idx[opaque])                                   # back into the colours the bowl and its contents use
        hsv = np.array([colorsys.rgb_to_hsv(*(c / 255)) for c in a.reshape(-1, 3)]).reshape(a.shape)
        h, s, v = hsv[..., 0], hsv[..., 1], hsv[..., 2]
        yy = np.mgrid[0:idx.shape[0], 0:idx.shape[1]][0]
        if name == "bowl_food":
            inside = rows_filled(biggest(opaque & (s > 0.25) & (v < 0.62) & ((h < 0.12) | (h > 0.95))))   # the kibble
        else:
            ys, xs = np.nonzero(opaque)
            cy, cx = ys.min() + (ys.max() - ys.min()) * 0.38, (xs.min() + xs.max()) / 2   # the rim's opening (measured on the prop: 13.3 of 33, 17 of 35)
            inside = ellipse(idx.shape, cy, cx, (ys.max() - ys.min()) * 0.19, (xs.max() - xs.min()) * 0.29) & opaque
        ys, xs = np.nonzero(inside); cy, cx = ys.mean(), xs.mean()
        ry, rx = (ys.max() - ys.min()) / 2 + .5, (xs.max() - xs.min()) / 2 + .5
        t = ((yy - (cy - ry)) / (2 * ry)).clip(0, 1)
        for k, frac in ((3, 1.0), (2, 0.72), (1, 0.45), (0, 0.0)) if name == "bowl_water" else ((2, 0.72), (1, 0.45), (0, 0.0)):   # the water's own picture shows no water: a full one too
            b = a.copy()
            keep = inside & ellipse(idx.shape, cy + ry * (1 - frac) * 0.55, cx, ry * frac, rx * frac) if frac > 0 else np.zeros_like(inside)
            if name == "bowl_food":
                wall = np.stack([150 + 70 * t, 140 + 68 * t, 128 + 64 * t], -1)
                m = inside & ~keep; b[m] = wall[m]
                edge = keep & ~(np.roll(keep, 1, 0) & np.roll(keep, -1, 0) & np.roll(keep, 1, 1) & np.roll(keep, -1, 1))
                b[edge] *= 0.85
            else:
                dry = np.stack([46 + 40 * t, 66 + 50 * t, 98 + 48 * t], -1); wet = np.stack([118 + 30 * t, 172 + 26 * t, 214 + 18 * t], -1)
                b[inside] = dry[inside]; b[keep] = wet[keep]
                if keep.any():
                    ky, kx = np.nonzero(keep); y0 = ky.min() + max(1, (ky.max() - ky.min()) // 3); x = np.nonzero(keep[y0])[0]
                    if len(x) > 4: b[y0, x[1]:x[1] + max(2, len(x) // 3)] = (235, 248, 255)
            cand = pal[1:]                                              # nearest of the room's colours (0 = transparent)
            d = ((b[..., None, :] - cand[None, None]) ** 2).sum(-1)
            out = np.where(opaque, d.argmin(-1) + 1, 0).astype(np.uint8)
            out = np.where(opaque & ~inside, idx, out)                  # outside the inside: the original pixels exactly
            im = Image.fromarray(out, "P"); im.putpalette(pal.astype(np.uint8).flatten().tolist()); im.info["transparency"] = 0
            im.save(os.path.join(G, f"{name}_{k}.png"), transparency=0)
        print(f"{name}: inside {int(inside.sum())} px -> {name}_2 / _1 / _0")

if __name__ == "__main__":
    main()
