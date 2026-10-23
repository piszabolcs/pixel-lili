#!/usr/bin/env python3
"""Pixel Lili: clean the approved 2D Lili poses into crisp game sprites (2026-10-23, the 2D line).

    py -3.12 tools/sprite_clean.py              docs/art/Lilik -> assets/sprites/lili_src/*.png + docs/img/lili_src_compare.png

The inputs are AI pixel-art images that were resampled on the way (pixellab 128 px PNGs: the drawing's own
pixel is ~1.17 px; the Gemini montages: a 240 px drawing blown up 4.27x into a JPG). That resampling is
why they look soft. What makes a sprite crisp is the reverse: every pixel on one grid, few colours, hard
edges. So, per pose (assets/sprites/lili_src/poses.json says which file, which part, what it is):
  1. the montages: the beige background (and the baked ground shadow, spec 7.5 rule 6) is removed by a
     flood fill from the border over background-like colours; the dark outline stops it at the dog
  2. resampled onto the output grid: one output pixel = the MOST COMMON source colour of the source
     pixels whose centres fall in it (no averaging, so no mush), opaque if more than half is covered
  3. every pixel mapped to Lili's own 40 colours (k-means in Oklab over all the poses, so the warm fawn
     and the soft pinks stay as drawn; the master palette's ramps made them dull), kept in the 256 palette's
     free slots 144-183 (assets/palettes/lili_sprites.json), the existing colours untouched
  4. a clean 1 px outline (the colour the sources' edges are drawn in) on the silhouette's edge; stray dark pixels just
     inside it removed; specks under 15 px (shadow remnants) dropped
  5. into a 96 x 96 cell, the paws' centre on the pivot (48, 90); one scale for all poses of a source,
     the two sources matched by a sitting pose's height
"""
import json, os, sys
import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from palette_from_photos import rgb_to_oklab, oklab_to_rgb, kmeans   # the one Oklab

SRC = os.path.join(ROOT, "docs", "art", "Lilik")
OUT = os.path.join(ROOT, "assets", "sprites", "lili_src")
CELL, PIVOT = 96, (48, 90)
SPRITE_SLOT, SPRITE_K = 144, 40          # Lili's own colours: the 256 palette's free slots 144-183 (16 ramps x 8 end at 143)

def sprite_palette(sources, pal_rgb):
    """k-means over every opaque source pixel of every pose -> SPRITE_K colours, darkest first, into the free slots"""
    px = np.concatenate([src[..., :3][src[..., 3] > 200] for src in sources])
    rng = np.random.default_rng(7)
    px = px[rng.choice(len(px), min(len(px), 120000), replace=False)]
    cs, share = kmeans(rgb_to_oklab(px), SPRITE_K, iters=40, seed=7)
    cs = cs[np.argsort(cs[:, 0])]
    rgb = np.clip(np.round(oklab_to_rgb(cs)), 0, 255).astype(np.uint8)
    full = pal_rgb.copy(); full[SPRITE_SLOT:SPRITE_SLOT + SPRITE_K] = rgb
    json.dump({"slots": [SPRITE_SLOT, SPRITE_SLOT + SPRITE_K - 1], "rgb": rgb.tolist(),
               "_note": "Lili's sprite colours (tools/sprite_clean.py, k-means in Oklab over docs/art/Lilik), darkest first"},
              open(os.path.join(ROOT, "assets", "palettes", "lili_sprites.json"), "w", encoding="utf-8"), indent=1)
    # the outline colour as drawn (a dark plum brown, not black): the darkest quarter of a 2 px ring inside the
    # silhouette, from the poses with a real alpha (the montages' edge is JPEG-blended with the beige)
    edge = []
    for src in sources:
        if (src[..., 3] == 255).all() or ((src[..., 3] > 0) & (src[..., 3] < 255)).sum() == 0:
            continue
        a = src[..., 3] > 200; core = a.copy()
        for _ in range(2):
            n = np.zeros_like(core); n[1:] |= ~core[:-1]; n[:-1] |= ~core[1:]; n[:, 1:] |= ~core[:, :-1]; n[:, :-1] |= ~core[:, 1:]
            core &= ~n
        edge.append(src[..., :3][a & ~core])
    e = rgb_to_oklab(np.concatenate(edge))
    e_lab = np.median(e[e[:, 0] <= np.percentile(e[:, 0], 25)], axis=0)
    full_lab = rgb_to_oklab(full)
    slots = np.arange(SPRITE_SLOT, SPRITE_SLOT + SPRITE_K)
    outline = int(slots[((full_lab[slots] - e_lab) ** 2).sum(1).argmin()])
    return full, full_lab, slots, outline

def load_palette():
    m = json.load(open(os.path.join(ROOT, "assets", "palettes", "master.json"), encoding="utf-8"))
    rgb = np.array(m["rgb"], dtype=np.uint8)
    ramps = m["ramps"]
    allowed = [1, 2, 3, 7, 8, 9, 10, 12, 13]                                     # outline, black, white, eye, nose, ear pink, sun, shadow, sparkle
    for r in ("fur", "fur_w", "metal", "water", "flower", "blanket"):           # the dog, the bowl, the tongue
        k = ramps.index(r); allowed += list(range(16 + 8 * k, 24 + 8 * k))
    return rgb, rgb_to_oklab(rgb), np.array(sorted(set(allowed)))

def remove_background(rgba):
    """the montages: flood fill from the border over background-like pixels (the beige and its darker shadow)"""
    lab = rgb_to_oklab(rgba[..., :3])
    H, W = lab.shape[:2]
    border = np.concatenate([lab[0], lab[-1], lab[:, 0], lab[:, -1]])
    bg = np.median(border, axis=0)
    dab = np.hypot(lab[..., 1] - bg[1], lab[..., 2] - bg[2])
    bgish = (dab < 0.030) & (lab[..., 0] > bg[0] - 0.24) & (lab[..., 0] < bg[0] + 0.05)
    mask = np.zeros((H, W), bool)
    mask[0], mask[-1], mask[:, 0], mask[:, -1] = bgish[0], bgish[-1], bgish[:, 0], bgish[:, -1]
    while True:                                                                  # iterative 4-neighbour flood
        grown = mask.copy()
        grown[1:] |= mask[:-1]; grown[:-1] |= mask[1:]; grown[:, 1:] |= mask[:, :-1]; grown[:, :-1] |= mask[:, 1:]
        grown &= bgish
        if (grown == mask).all(): break
        mask = grown
    out = rgba.copy(); out[..., 3] = np.where(mask, 0, 255)
    return out

def split_columns(rgba, min_gap=6, min_width=40):
    """the montage's dogs stand apart: split at runs of empty columns; -> list of (x0, x1)"""
    occ = (rgba[..., 3] > 0).sum(0) > 2
    spans, x, W = [], 0, len(occ)
    while x < W:
        if occ[x]:
            x0 = x
            while x < W and (occ[x] or occ[min(W - 1, x + 1):min(W, x + min_gap)].any()): x += 1
            if x - x0 >= min_width: spans.append((x0, x))
        x += 1
    return spans

def bbox(alpha):
    ys, xs = np.nonzero(alpha)
    return xs.min(), ys.min(), xs.max() + 1, ys.max() + 1

def resample(rgba, cell, pal_lab, allowed):
    """source -> output grid: each output pixel = the most common palette colour of the covered source pixels
    whose centres fall in it; opaque if > 50 % covered"""
    H, W = rgba.shape[:2]
    ow, oh = int(np.ceil(W / cell)), int(np.ceil(H / cell))
    yy, xx = np.mgrid[0:H, 0:W]
    oxi = np.minimum(((xx + 0.5) / cell).astype(int), ow - 1)
    oyi = np.minimum(((yy + 0.5) / cell).astype(int), oh - 1)
    cov = rgba[..., 3] > 127
    idx = np.zeros((H, W), np.int32)
    if cov.any():
        lab = rgb_to_oklab(rgba[..., :3][cov])
        d = ((lab[:, None, :] - pal_lab[None, allowed, :]) ** 2).sum(2)
        idx[cov] = allowed[d.argmin(1)]
    cell_id = oyi * ow + oxi
    counts = np.zeros((ow * oh, 256), np.int32)
    np.add.at(counts, (cell_id[cov], idx[cov]), 1)
    total = np.bincount(cell_id.ravel(), minlength=ow * oh)
    opaque = counts.sum(1) * 2 > total
    out = np.where(opaque, counts.argmax(1), 0).astype(np.uint8).reshape(oh, ow)
    return out

def drop_specks(img, min_px=15):
    """8-connected pieces smaller than min_px (shadow and JPEG remnants) become transparent"""
    H, W = img.shape; seen = np.zeros((H, W), bool); out = img.copy()
    for y0, x0 in zip(*np.nonzero(img)):
        if seen[y0, x0]: continue
        stack, comp = [(y0, x0)], []; seen[y0, x0] = True
        while stack:
            y, x = stack.pop(); comp.append((y, x))
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < H and 0 <= xx < W and img[yy, xx] and not seen[yy, xx]:
                        seen[yy, xx] = True; stack.append((yy, xx))
        if len(comp) < min_px:
            for y, x in comp: out[y, x] = 0
    return out

def clean_outline(img, pal_lab, outline_idx):
    """a continuous 1 px PAL_OUTLINE on the silhouette's inner edge; dark pixels right inside it that are not
    a feature (an eye has dark neighbours all round, a doubled outline does not) take their inner neighbour's colour"""
    body = img != 0
    pad = np.pad(body, 1)
    inner = pad[1:-1, 1:-1] & ~(pad[:-2, 1:-1] & pad[2:, 1:-1] & pad[1:-1, :-2] & pad[1:-1, 2:])
    out = img.copy()
    out[inner] = outline_idx
    L = pal_lab[:, 0]
    dark = (L[out] < 0.33) & body & ~inner
    pin = np.pad(inner, 1)
    near_edge = pin[:-2, 1:-1] | pin[2:, 1:-1] | pin[1:-1, :-2] | pin[1:-1, 2:]
    H, W = out.shape
    for y, x in zip(*np.nonzero(dark & near_edge)):
        neigh = [out[y + dy, x + dx] for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1))
                 if 0 <= y + dy < H and 0 <= x + dx < W and out[y + dy, x + dx] != 0 and not inner[y + dy, x + dx] and L[out[y + dy, x + dx]] >= 0.33]
        if neigh:
            out[y, x] = max(set(neigh), key=neigh.count)
    return out

def place(img):
    """into the 96 x 96 cell, the silhouette's bottom on the pivot row, centred on the pivot column"""
    x0, y0, x1, y1 = bbox(img != 0)
    crop = img[y0:y1, x0:x1]
    h, w = crop.shape
    cell = np.zeros((CELL, CELL), np.uint8)
    ox, oy = PIVOT[0] - w // 2, PIVOT[1] - h
    if ox < 0 or oy < 0 or ox + w > CELL:
        raise ValueError(f"does not fit the {CELL} px cell: {w} x {h}")
    cell[oy:oy + h, ox:ox + w] = crop
    return cell, (w, h)

def save_indexed(path, idx, pal_rgb):
    im = Image.fromarray(idx, mode="P"); im.putpalette(pal_rgb.flatten().tolist())
    im.save(path, transparency=0)

def main():
    poses = json.load(open(os.path.join(OUT, "poses.json"), encoding="utf-8"))
    pal_rgb, pal_lab, allowed = load_palette()
    scale = poses["_scale"]
    # the montage dogs, cut once per file
    cuts = {}
    for f in sorted({p["file"] for p in poses["poses"] if p.get("part") is not None}):
        rgba = remove_background(np.asarray(Image.open(os.path.join(SRC, f)).convert("RGBA")).copy())
        cuts[f] = (rgba, split_columns(rgba))
        print(f"{f}: {len(cuts[f][1])} dogs at x {cuts[f][1]}")
    # one cell size per source group: the 128 px PNGs by the reference pose, the montages matched by a sitting pose's height
    def src_of(p):
        if p.get("part") is None:
            return np.asarray(Image.open(os.path.join(SRC, p["file"])).convert("RGBA"))
        rgba, spans = cuts[p["file"]]; x0, x1 = spans[p["part"]]
        sub = rgba[:, x0:x1]; bx0, by0, bx1, by1 = bbox(sub[..., 3] > 0)
        return sub[by0:by1, bx0:bx1]
    by_id = {p["id"]: p for p in poses["poses"]}
    ref_png = src_of(by_id[scale["png_ref"]]); _, _, _, h_png = bbox(ref_png[..., 3] > 127); h_png -= bbox(ref_png[..., 3] > 127)[1]
    cell_png = h_png / scale["png_ref_height"]
    ref_m = src_of(by_id[scale["montage_ref"]]); h_m = bbox(ref_m[..., 3] > 127)[3] - bbox(ref_m[..., 3] > 127)[1]
    cell_m = h_m / scale["png_ref_height"] * scale["montage_ref_ratio"]
    print(f"cell: PNG {cell_png:.3f} src px, montage {cell_m:.3f} src px per output px")
    os.makedirs(OUT, exist_ok=True)
    sources = [src_of(p) for p in poses["poses"]]
    pal_rgb, pal_lab, allowed, outline_idx = sprite_palette(sources, pal_rgb)
    print(f"outline colour: slot {outline_idx} #%02x%02x%02x" % tuple(pal_rgb[outline_idx]))
    report, tiles = [], []
    for p, src in zip(poses["poses"], sources):
        img = resample(src, cell_png if p.get("part") is None else cell_m, pal_lab, allowed)
        img = drop_specks(img)
        img = clean_outline(img, pal_lab, outline_idx)
        try:
            cell, (w, h) = place(img)
        except ValueError as e:
            raise SystemExit(f"{p['id']}: {e}")
        save_indexed(os.path.join(OUT, p["id"] + ".png"), cell, pal_rgb)
        colours = len(np.unique(cell[cell != 0]))
        report.append(f"  {p['id']:<14} {w:>3} x {h:<3} {colours:>2} colours  <- {p['file']}" + (f" #{p['part']}" if p.get("part") is not None else ""))
        tiles.append((p, src, cell))
    print("\n".join(report))
    # before / after: the source scaled to the same size (smooth), the result at 4x nearest
    S = 4; T = CELL * S
    sheet = Image.new("RGB", (len(tiles) * (T + 8) + 8, 2 * (T + 8) + 30), (40, 38, 46))
    d = ImageDraw.Draw(sheet)
    for i, (p, src, cell) in enumerate(tiles):
        x = 8 + i * (T + 8)
        chk = Image.new("RGB", (T, T), (150, 150, 150))
        for yy in range(0, T, 32):
            for xx in range(0, T, 32):
                if (xx // 32 + yy // 32) % 2: chk.paste((120, 120, 120), (xx, yy, xx + 32, yy + 32))
        before = Image.fromarray(src).convert("RGBA")
        k = min(T / before.width, T / before.height) * 0.92
        before = before.resize((max(1, int(before.width * k)), max(1, int(before.height * k))), Image.BILINEAR)
        b = chk.copy(); b.paste(before, ((T - before.width) // 2, T - before.height - 8), before)
        a = Image.fromarray(cell, "P"); a.putpalette(pal_rgb.flatten().tolist())
        a = a.convert("RGBA"); al = np.asarray(a).copy(); al[..., 3] = np.where(cell != 0, 255, 0); a = Image.fromarray(al).resize((T, T), Image.NEAREST)
        c = chk.copy(); c.paste(a, (0, 0), a)
        sheet.paste(b, (x, 8)); sheet.paste(c, (x, T + 16))
        d.text((x + 4, 2 * T + 18), p["id"], fill=(230, 226, 220))
    out = os.path.join(ROOT, "docs", "img", "lili_src_compare.png")
    sheet.save(out)
    print(f"wrote {len(tiles)} sprites -> {os.path.relpath(OUT, ROOT)}, compare -> {os.path.relpath(out, ROOT)}")

if __name__ == "__main__":
    main()
