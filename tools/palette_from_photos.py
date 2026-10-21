#!/usr/bin/env python3
"""Pixel Lili: the master palette from the approved concept art and Lili's photos (B0, spec 7.5, M3.2).

    py -3.12 tools/palette_from_photos.py                      -> assets/palettes/master.gpl, master.json, docs/img/m3_2_palette.png
    py -3.12 tools/palette_from_photos.py --probe <image> x0 y0 x1 y1 [k]    the k dominant colours of a region (to pick SOURCES)

How it works:
  1. SOURCES names, per material, a rectangle on an approved image (reference/concept) or photo
     and which k-means cluster is the material's base colour (an index by share, a pick() rule,
     or an anchor hex: the cluster nearest to it) (k-means in Oklab, so "dominant"
     means perceptually dominant, not RGB-average mush).
  2. From the base colour an 8-shade ramp is built in Oklab (palette.h: dark -> light): the
     lightness steps are even, the shadows drift towards blue and the highlights towards yellow
     (the hi-bit look of the concept art, spec 7.1), chroma eases off at both ends.
  3. The fixed slots 0-15 and the 16 ramps (palette.h order = tools/gen_maps.py RAMPS) are
     written as a GIMP palette (.gpl, 256 lines, one name each), a JSON (the bases, for the
     generators), and a swatch sheet for the approval gate.
Nothing here is applied to the game until the sheet is approved (then pal_master in palette.c).
"""
import json, math, os, sys
import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOM = "reference/concept/room_approved_2026-10-21.jpg"
SHEET = "reference/concept/lili_sheet_approved_2026-10-21.jpg"
RUG = "reference/photos/rug_owner_room_2026-10-19.jpg"
RAMPS = ["floor", "wall", "grass", "hedge", "fur", "fur_w", "wood", "blanket", "metal",
         "water", "pad", "house", "flower", "leaf", "rug_green", "rug_gray"]

# ---- Oklab (Bjorn Ottosson), the space every comparison and ramp lives in ----
def srgb_to_linear(c):
    c = np.asarray(c, dtype=np.float64) / 255.0
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
def linear_to_srgb(c):
    c = np.clip(c, 0, 1)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055) * 255.0
M1 = np.array([[0.4122214708, 0.5363325363, 0.0514459929], [0.2119034982, 0.6806995451, 0.1073969566], [0.0883024619, 0.2817188376, 0.6299787005]])
M2 = np.array([[0.2104542553, 0.7936177850, -0.0040720468], [1.9779984951, -2.4285922050, 0.4505937099], [0.0259040371, 0.7827717662, -0.8086757660]])
def rgb_to_oklab(rgb):
    lms = srgb_to_linear(rgb) @ M1.T
    return np.cbrt(lms) @ M2.T
def oklab_to_rgb(lab):
    lms = (np.asarray(lab) @ np.linalg.inv(M2).T) ** 3
    return linear_to_srgb(lms @ np.linalg.inv(M1).T)

def kmeans(px, k, iters=30, seed=1):
    rng = np.random.default_rng(seed)
    c = px[rng.choice(len(px), k, replace=False)]
    for _ in range(iters):
        d = ((px[:, None, :] - c[None, :, :]) ** 2).sum(2)
        a = d.argmin(1)
        for i in range(k):
            if (a == i).any():
                c[i] = px[a == i].mean(0)
    share = np.bincount(a, minlength=k) / len(px)
    order = np.argsort(-share)
    return c[order], share[order]

def region(path, box, k=4, step=2):
    im = Image.open(os.path.join(ROOT, path)).convert("RGB")
    x0, y0, x1, y1 = box
    a = np.asarray(im.crop((x0, y0, x1, y1)))[::step, ::step].reshape(-1, 3)
    return kmeans(rgb_to_oklab(a), k)

def hexs(lab):
    r, g, b = [int(round(v)) for v in oklab_to_rgb(lab)]
    return f"#{r:02x}{g:02x}{b:02x}"

def hue(c): return math.degrees(math.atan2(c[2], c[1])) % 360
def chroma(c): return math.hypot(c[1], c[2])

# ---- the ramp: 8 shades around a base colour, dark -> light (palette.h: shade 0 dark, 4 = base) ----
def ramp8(base_lab, dark=0.075, light=0.045, cool=0.22, warm=0.10, base_shade=4, min_chroma=0.0):
    L, a, b = base_lab
    C, h = max(chroma(base_lab), min_chroma), math.atan2(b, a)      # min_chroma: a washed-out sample made a proper material colour
    BLUE, YELLOW = math.radians(264), math.radians(100)
    def towards(h0, h1, f):
        return h0 + (((h1 - h0 + math.pi) % (2 * math.pi)) - math.pi) * f       # the short way round
    out = []
    for i in range(8):
        t = i - base_shade
        if t < 0:
            Li = L + t * dark
            hi = towards(h, BLUE, cool * (-t) / base_shade)
            Ci = C * (1.0 - 0.35 * (-t) / base_shade)
        else:
            n = max(1, 7 - base_shade)
            Li = L + t * light
            hi = towards(h, YELLOW, warm * t / n)
            Ci = C * (1.0 - 0.45 * t / n)
        Li = min(max(Li, 0.08), 0.985)
        out.append(np.array([Li, Ci * math.cos(hi), Ci * math.sin(hi)]))
    return out

# ---- where each material's colour comes from: (image, box, k, cluster index by share or None = pick(), ramp tweaks) ----
SOURCES = {
    "floor":     (ROOM, (770, 1352, 936, 1560), 4, "#b8794a", {}),           # floorboards left of the dog, half lit: the cluster nearest this warm brown
    "wall":      (ROOM, (156, 416, 676, 603), 3, 0, {}),                     # the back wall between the plates and the dado
    "grass":     (ROOM, (960, 150, 1100, 330), 5, "#b0b070", {"dark": 0.07}),  # the garden through the door: olive greens (autumn)
    "hedge":     (ROOM, (960, 150, 1100, 330), 5, "#8a8a50", {}),
    "fur":       (SHEET, (100, 100, 330, 480), 6, None, {"light": 0.04}),    # the front dog, picked by hue
    "fur_w":     (SHEET, (100, 100, 330, 480), 6, None, {"dark": 0.06}),
    "wood":      (ROOM, (1279, 676, 1539, 770), 3, "#b06a44", {}),           # the bench seat, its lit wood
    "blanket":   (ROOM, (1602, 1082, 1810, 1248), 5, None, {}),              # the folk blanket's red
    "metal":     (ROOM, (1550, 1740, 1640, 1830), 3, "#d8d0c4", {}),         # the food bowl's light ceramic rim
    "water":     (ROOM, (1706, 1747, 1810, 1830), 3, None, {}),              # the water bowl's blue
    "pad":       (ROOM, (200, 1620, 530, 1800), 3, 0, {"dark": 0.05, "light": 0.02}),
    "house":     (ROOM, (790, 156, 894, 624), 3, "#d59670", {}),             # the door frame's warm wood (the yard's house wall later)
    "flower":    (ROOM, (930, 1530, 1150, 1615), 6, None, {"min_chroma": 0.085}),   # the petals: the light pink, made a proper pink
    "leaf":      (ROOM, (930, 1530, 1150, 1615), 6, None, {}),              # ... and the greenest
    "rug_green": (ROOM, (177, 749, 718, 1425), 6, "#8fa37a", {"min_chroma": 0.045}),  # the approved rug: sage green (the photo's is greyer)
    "rug_gray":  (ROOM, (177, 749, 718, 1425), 6, None, {"dark": 0.09, "light": 0.03}),
}

def pick(name, cs):
    """clusters -> the base for a material whose cluster index is None: by hue/lightness rules"""
    if name == "fur":       # warm orange-tan, mid lightness, with chroma
        return max((c for c in cs if 40 < hue(c) < 90 and 0.55 < c[0] < 0.9 and chroma(c) > 0.06), key=chroma)
    if name == "fur_w":     # the lightest low-chroma cluster
        return max((c for c in cs if chroma(c) < 0.06), key=lambda c: c[0])
    if name == "blanket":   # the reddest
        return max((c for c in cs if chroma(c) > 0.08), key=lambda c: -abs(((hue(c) + 180) % 360) - 180 - 25))
    if name == "flower":    # the light pink petals (the pot is browner and darker)
        return max((c for c in cs if chroma(c) > 0.035 and c[0] > 0.7 and (hue(c) > 320 or hue(c) < 30)), key=chroma)
    if name in ("leaf", "grass"):   # the greenest (olive counts: the autumn garden)
        return max((c for c in cs if 92 < hue(c) < 170 and c[0] < 0.9), key=chroma)
    if name == "hedge":     # the darkest green
        return min((c for c in cs if 92 < hue(c) < 170), key=lambda c: c[0])
    if name == "water":     # the bluest
        return max((c for c in cs if 200 < hue(c) < 290), key=chroma)
    if name == "rug_green":
        return max((c for c in cs if 90 < hue(c) < 165), key=chroma)
    if name == "rug_gray":  # the mid grey of the rug (off-white = shade 7, dark grey = shade 1)
        greys = sorted((c for c in cs if chroma(c) < 0.04), key=lambda c: c[0])
        return greys[len(greys) // 2] if len(greys) > 1 else greys[0]
    raise KeyError(name)

FIXED = {0: ("clear", "#000000"), 1: ("outline", None), 2: ("black", "#0f0c10"), 3: ("white", "#fffaf2"), 4: ("debug", "#ff00ff"),
         5: ("ui_glass", "#e8eef8"), 6: ("ui_text", "#1c1a22"), 7: ("eye", "#1a1416"), 8: ("nose", None), 9: ("ear_pink", None),
         10: ("sun", "#ffd98a"), 11: ("moon", "#9fb4e8"), 12: ("shadow", "#2a2238"), 13: ("sparkle", "#fff6c8"), 14: ("spare", "#808080"), 15: ("spare", "#808080")}

def build():
    rug_im = Image.open(os.path.join(ROOT, RUG)).convert("RGB")
    rug_box = (int(rug_im.width * 0.1), int(rug_im.height * 0.3), int(rug_im.width * 0.9), int(rug_im.height * 0.7))
    pal, names = [[0, 0, 0] for _ in range(256)], [""] * 256
    bases, log = {}, []
    for ri, name in enumerate(RAMPS):
        img, box, k, idx, tw = SOURCES[name]
        cs, sh = region(img, box or rug_box, k)
        if isinstance(idx, str):      # an anchor colour: the cluster nearest to it (Oklab distance)
            an = rgb_to_oklab([int(idx[j:j + 2], 16) for j in (1, 3, 5)])
            base = min(cs, key=lambda c: ((c - an) ** 2).sum())
        else:
            base = cs[idx] if idx is not None else pick(name, cs)
        bases[name] = hexs(base)
        for s, lab in enumerate(ramp8(base, **tw)):
            r, g, b = [int(round(v)) for v in oklab_to_rgb(lab)]
            pal[16 + ri * 8 + s] = [r, g, b]; names[16 + ri * 8 + s] = f"{name}.{s}"
        log.append(f"{name:<10} base {bases[name]}  from {os.path.basename(img)}: " + " ".join(hexs(c) + f"({p:.0%})" for c, p in zip(cs, sh)))
    # the fixed slots that come from the sheet: the outline, the nose, the ear pink
    cs, sh = region(SHEET, (100, 100, 330, 480), 8)
    outline = min(cs, key=lambda c: c[0])
    pinks = [c for c in cs if chroma(c) > 0.05 and (hue(c) > 330 or hue(c) < 30)]
    nose = min(pinks, key=lambda c: c[0]) if pinks else outline
    ear = max(pinks, key=lambda c: c[0]) if pinks else nose
    fx = dict(FIXED); fx[1] = ("outline", hexs(outline)); fx[8] = ("nose", hexs(nose)); fx[9] = ("ear_pink", hexs(ear))
    for i, (n, h) in fx.items():
        pal[i] = [int(h[j:j + 2], 16) for j in (1, 3, 5)]; names[i] = n
    log.append(f"fixed: outline {fx[1][1]}, nose {fx[8][1]}, ear {fx[9][1]} (from the sheet's front dog)")
    return pal, names, bases, log

def write_gpl(path, pal, names):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("GIMP Palette\nName: Pixel Lili master\nColumns: 8\n# generated by tools/palette_from_photos.py from reference/concept + photos; 0 = transparent, 1-15 fixed, then 16 ramps x 8 (palette.h)\n")
        for i in range(256):
            r, g, b = pal[i]
            f.write(f"{r:3d} {g:3d} {b:3d}\t{names[i] or f'unused.{i}'}\n")

def swatch(path, pal, names, bases):
    cell, left, top = 44, 150, 40
    rows = 1 + len(RAMPS)
    im = Image.new("RGB", (left + 8 * cell + 20, top + rows * (cell + 6) + 20), (30, 28, 34))
    d = ImageDraw.Draw(im)
    d.text((10, 12), "Pixel Lili master palette: 16 ramps x 8 shades (dark -> light), row 0 = fixed slots 0-7", fill=(230, 226, 220))
    for i in range(8):
        d.rectangle([left + i * cell, top, left + (i + 1) * cell - 2, top + cell - 2], fill=tuple(pal[i]))
        d.text((left + i * cell + 2, top + cell - 14), names[i][:7], fill=(255, 255, 255) if sum(pal[i]) < 380 else (0, 0, 0))
    d.text((10, top + 14), "fixed 0-7", fill=(230, 226, 220))
    for ri, name in enumerate(RAMPS):
        y = top + (ri + 1) * (cell + 6)
        d.text((10, y + 14), f"{name} {bases[name]}", fill=(230, 226, 220))
        for s in range(8):
            d.rectangle([left + s * cell, y, left + (s + 1) * cell - 2, y + cell - 2], fill=tuple(pal[16 + ri * 8 + s]))
    im.resize((im.width * 2, im.height * 2), Image.NEAREST).save(path)

def main():
    if len(sys.argv) > 1 and sys.argv[1] == "--probe":
        img, x0, y0, x1, y1 = sys.argv[2], *map(int, sys.argv[3:7])
        k = int(sys.argv[7]) if len(sys.argv) > 7 else 4
        cs, sh = region(img, (x0, y0, x1, y1), k)
        for c, p in zip(cs, sh):
            print(f"  {hexs(c)}  {p:5.1%}  L {c[0]:.2f} hue {hue(c):5.0f} C {chroma(c):.3f}")
        return
    pal, names, bases, log = build()
    os.makedirs(os.path.join(ROOT, "assets", "palettes"), exist_ok=True)
    write_gpl(os.path.join(ROOT, "assets", "palettes", "master.gpl"), pal, names)
    json.dump({"ramps": RAMPS, "bases": bases, "fixed": {names[i]: "#%02x%02x%02x" % tuple(pal[i]) for i in range(16)}, "rgb": pal},
              open(os.path.join(ROOT, "assets", "palettes", "master.json"), "w", encoding="utf-8"), indent=1)
    swatch(os.path.join(ROOT, "docs", "img", "m3_2_palette.png"), pal, names, bases)
    print("\n".join(log))
    print("wrote assets/palettes/master.gpl, master.json, docs/img/m3_2_palette.png")

if __name__ == "__main__":
    main()
