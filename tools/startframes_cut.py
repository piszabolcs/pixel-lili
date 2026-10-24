#!/usr/bin/env python3
"""Pixel Lili: the Gemini start-frame sheets -> 96 px start frames for PixelLab (2026-10-23).

    py -3.12 tools/startframes_cut.py --cells      docs/art/Lilik/0N.jpg -> the raw cells, numbered, on one sheet (to tell the directions)
    py -3.12 tools/startframes_cut.py              the cells listed in docs/art/startframes/cells.json -> assets/startframes/<pose>_<dir>.png

The sheets (docs/art/startframes/PROMPTS.md) are 8 dogs on flat magenta in 2 rows of 4; Gemini does not always
draw the asked direction, so cells.json says what each cell really is. Per cell:
  1. the magenta is keyed out everywhere, not flood-filled from the border, so a hole the tail closes against
     the body goes too (the old N walk kept one: the owner, 2026-10-23)
  2. resampled onto the 96 px masters' grid (tools/sprite_clean.py resample: the most common colour per output
     pixel, no mush), one scale for all sheets, from a sitting dog's height against the approved sit_S master
  3. mapped to Lili's 40 colours plus a few for the props (the rope, the biscuit, the bowls), a clean outline
  4. into a 128 x 128 cell, the lowest pixel on the row above the pivot (64, 122), centred: the same dog as the
     96 px masters with 40 px of room above her (a play bow's tail, a jump), so PixelLab has space to move
"""
import argparse, json, os, sys
import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from palette_from_photos import rgb_to_oklab, oklab_to_rgb, kmeans
import sprite_clean as sc

SRC = os.path.join(ROOT, "docs", "art", "Lilik")
OUT = os.path.join(ROOT, "assets", "startframes")
CELLS = os.path.join(ROOT, "docs", "art", "startframes", "cells.json")
CELL, PIVOT, SLOT = 128, (64, 122), 144
SHEETS = [f"0{i}.jpg" for i in range(1, 8)]

def key_magenta(rgb):
    """background = magenta-ish: red and blue high, green well under both (JPEG noise allowed)"""
    r, g, b = (rgb[..., i].astype(int) for i in range(3))
    return (r > 150) & (b > 150) & (g < 0.62 * np.minimum(r, b))

def pieces(alpha, min_area=600):
    """the dogs: rows split at empty rows, then each row at empty columns -> boxes (x0, y0, x1, y1), reading order"""
    occ_r = alpha.sum(1) > 3
    rows, y = [], 0
    while y < len(occ_r):
        if occ_r[y]:
            y0 = y
            while y < len(occ_r) and occ_r[max(0, y - 6):y + 6].any(): y += 1
            if y - y0 > 30: rows.append((y0, y))
        y += 1
    boxes = []
    for y0, y1 in rows:
        sub = alpha[y0:y1]
        occ = sub.sum(0) > 2
        x = 0
        while x < len(occ):
            if occ[x]:
                x0 = x
                while x < len(occ) and occ[x:x + 8].any(): x += 1
                if x - x0 > 25:
                    ys = np.nonzero(sub[:, x0:x].any(1))[0]
                    if sub[:, x0:x].sum() >= min_area: boxes.append((x0, y0 + ys.min(), x, y0 + ys.max() + 1))
            x += 1
    return boxes

def load_sheet(f):
    rgb = np.asarray(Image.open(os.path.join(SRC, f)).convert("RGB"))
    alpha = ~key_magenta(rgb)
    # specks of JPEG noise in the magenta: open by one pixel
    a = alpha.copy()
    n = np.zeros_like(a); n[1:] |= ~a[:-1]; n[:-1] |= ~a[1:]; n[:, 1:] |= ~a[:, :-1]; n[:, :-1] |= ~a[:, 1:]
    core = a & ~n
    g = core.copy(); g[1:] |= core[:-1]; g[:-1] |= core[1:]; g[:, 1:] |= core[:, :-1]; g[:, :-1] |= core[:, 1:]
    alpha = a & g
    rgba = np.dstack([rgb, np.where(alpha, 255, 0).astype(np.uint8)])
    return rgba

def cells_sheet():
    """every cell of every sheet, numbered sheet.cell, at a common height, for telling the directions"""
    tiles = []
    for f in SHEETS:
        if not os.path.exists(os.path.join(SRC, f)): print("missing", f); continue
        rgba = load_sheet(f)
        boxes = pieces(rgba[..., 3] > 0)
        print(f, len(boxes), "pieces:", [(x1 - x0, y1 - y0) for x0, y0, x1, y1 in boxes])
        for i, (x0, y0, x1, y1) in enumerate(boxes):
            tiles.append((f"{f[:2]}.{i + 1}", rgba[y0:y1, x0:x1]))
    T = 220
    cols = 8
    rows = (len(tiles) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * T, rows * (T + 22)), (60, 60, 70))
    d = ImageDraw.Draw(sheet)
    for k, (name, t) in enumerate(tiles):
        im = Image.fromarray(t, "RGBA")
        s = min((T - 10) / im.width, (T - 10) / im.height)
        im = im.resize((max(1, int(im.width * s)), max(1, int(im.height * s))), Image.NEAREST)
        x, y = (k % cols) * T, (k // cols) * (T + 22)
        sheet.paste(im, (x + (T - im.width) // 2, y + 22 + (T - im.height) // 2), im)
        d.text((x + 6, y + 4), name, fill=(255, 255, 120))
    out = os.path.join(ROOT, "docs", "img", "startframes_cells.png")
    sheet.save(out); print("->", out)

def build_palette(sources):
    """Lili's 40 colours as they are, plus up to 16 for what is not her (the rope, the biscuit, the bowls)"""
    lili = np.array(json.load(open(os.path.join(ROOT, "assets", "palettes", "lili_sprites.json"), encoding="utf-8"))["rgb"], float)
    lili_lab = rgb_to_oklab(lili)
    px = np.concatenate([s[..., :3][s[..., 3] > 0] for s in sources]).astype(float)
    lab = rgb_to_oklab(px)
    d = np.sqrt(((lab[:, None, :] - lili_lab[None]) ** 2).sum(2)).min(1)
    other = lab[d > 0.09]
    extra = np.zeros((0, 3))
    if len(other) > 200:
        rng = np.random.default_rng(3)
        other = other[rng.choice(len(other), min(len(other), 60000), replace=False)]
        cs, share = kmeans(other, 16, iters=30, seed=3)
        extra = np.clip(np.round(oklab_to_rgb(cs[share > 0.004])), 0, 255)
    pal = np.zeros((256, 3)); pal[SLOT:SLOT + 40] = lili
    pal[SLOT + 40:SLOT + 40 + len(extra)] = extra
    allowed = np.arange(SLOT, SLOT + 40 + len(extra))
    return pal.astype(np.uint8), rgb_to_oklab(pal), allowed

def outline_slot():
    """the outline as the game's masters have it: the commonest colour on idle_stand's silhouette edges"""
    d = os.path.join(ROOT, "assets", "sprites", "lili_96", "idle_stand")
    edge = []
    for f in os.listdir(d):
        if not f.endswith(".png"): continue
        a = np.asarray(Image.open(os.path.join(d, f))); b = a != 0; p = np.pad(b, 1)
        edge.append(a[b & ~(p[:-2, 1:-1] & p[2:, 1:-1] & p[1:-1, :-2] & p[1:-1, 2:])])
    return int(np.bincount(np.concatenate(edge)).argmax())

def master_height(name):
    a = np.asarray(Image.open(os.path.join(ROOT, "assets", "sprites", "lili_src", name + ".png")))
    ys = np.nonzero((a != 0).any(1))[0]
    return ys.max() - ys.min() + 1

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--cells", action="store_true"); a = ap.parse_args()
    if a.cells: return cells_sheet()
    spec = json.load(open(CELLS, encoding="utf-8"))
    sheets = {f: load_sheet(f) for f in SHEETS if os.path.exists(os.path.join(SRC, f))}
    boxes = {f: pieces(r[..., 3] > 0) for f, r in sheets.items()}
    def crop(ref):
        f, i = ref.split("."); f = f + ".jpg"; x0, y0, x1, y1 = boxes[f][int(i) - 1]
        return sheets[f][y0:y1, x0:x1]
    # one scale for every sheet: the reference sitting dog against the approved sit_S master
    ref = crop(spec["_scale_ref"]); h_ref = (ref[..., 3] > 0).any(1).sum()
    cell = h_ref / master_height("sit_S")
    print(f"scale: {spec['_scale_ref']} is {h_ref} px tall, sit_S {master_height('sit_S')} px -> {cell:.3f} source px per output px")
    todo = [(k, v) for k, v in spec.items() if not k.startswith("_")]
    sources = [crop(v["cell"]) for k, v in todo]
    pal, pal_lab, allowed = build_palette(sources)
    outline = outline_slot()
    os.makedirs(OUT, exist_ok=True)
    report, tiles = [], []
    for (name, v), src in zip(todo, sources):
        size = spec.get("_sheet_size", {}).get(v["cell"].split(".")[0], 1.0) * v.get("size", 1.0)   # a sheet Gemini drew bigger
        img = sc.resample(src, cell / size, pal_lab, allowed)
        img = sc.drop_specks(img)
        img = sc.clean_outline(img, pal_lab, outline)
        ys, xs = np.nonzero(img)
        crop_ = img[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
        h, w = crop_.shape
        out = np.zeros((CELL, CELL), np.uint8)
        ox, oy = PIVOT[0] - w // 2 + v.get("dx", 0), PIVOT[1] - h
        if ox < 0 or oy < 0 or ox + w > CELL:
            print(f"!! {name}: {w} x {h} does not fit the cell"); continue
        out[oy:oy + h, ox:ox + w] = crop_
        rgba = np.zeros((CELL, CELL, 4), np.uint8); rgba[..., :3] = pal[out]; rgba[..., 3] = np.where(out > 0, 255, 0)
        Image.fromarray(rgba, "RGBA").save(os.path.join(OUT, name + ".png"))
        holes = hole_check(out)
        report.append(f"  {name:<16} {w:>3} x {h:<3} <- {v['cell']}" + (f"  ({holes} px see-through hole, e.g. inside the tail's curl)" if holes else ""))
        tiles.append((name, rgba))
    print("\n".join(report))
    contact(tiles)

def hole_check(idx):
    """transparent pixels the outside cannot reach (a 4-connected flood from the border): enclosed background"""
    t = idx == 0; H, W = t.shape; seen = np.zeros_like(t)
    stack = [(y, x) for y in range(H) for x in (0, W - 1) if t[y, x]] + [(y, x) for x in range(W) for y in (0, H - 1) if t[y, x]]
    for y, x in stack: seen[y, x] = True
    while stack:
        y, x = stack.pop()
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            yy, xx = y + dy, x + dx
            if 0 <= yy < H and 0 <= xx < W and t[yy, xx] and not seen[yy, xx]: seen[yy, xx] = True; stack.append((yy, xx))
    return int((t & ~seen).sum())

def contact(tiles):
    S = 3; T = CELL * S; cols = 8
    rows = (len(tiles) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * (T + 6) + 6, rows * (T + 24) + 6), (40, 40, 48))
    d = ImageDraw.Draw(sheet)
    for k, (name, rgba) in enumerate(tiles):
        x, y = 6 + (k % cols) * (T + 6), 6 + (k // cols) * (T + 24)
        chk = Image.new("RGB", (T, T), (150, 150, 150))
        for yy in range(0, T, 24):
            for xx in range(0, T, 24):
                if (xx // 24 + yy // 24) % 2: chk.paste((125, 125, 125), (xx, yy, xx + 24, yy + 24))
        im = Image.fromarray(rgba, "RGBA").resize((T, T), Image.NEAREST)
        chk.paste(im, (0, 0), im); sheet.paste(chk, (x, y + 18))
        d.text((x + 2, y + 2), name, fill=(240, 240, 200))
    out = os.path.join(ROOT, "docs", "img", "startframes_contact.png")
    sheet.save(out); print("->", out)

if __name__ == "__main__":
    main()
