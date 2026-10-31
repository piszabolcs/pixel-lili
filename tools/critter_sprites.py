#!/usr/bin/env python3
"""Pixel Lili: the yard's small life (2026-10-31, the owner: "the yard is too static, it feels fake"): the sparrow and
the butterfly PixelLab drew on 2026-10-27 (the owner then: "the birds and the butterfly are good, later as particles";
the swaying tree was rejected), and the sparrow pecking on the ground (2026-10-31).

    py -3.12 tools/critter_sprites.py            assets/sprites/lili_yard/<set>, anims.json, docs/img/critters.png
    py -3.12 tools/critter_sprites.py --preview  docs/img/critters.png only

assets/pixellab/<sheet>_sheet.png: Pixelorama's image sequence (64 px cells: the canvas, then the 16 generated frames).
Each frame is resampled onto the game's grid at one scale for the whole set (so the frames keep their size and place),
in Lili's 40 colours and the rope's 6 (tools/sprite_clean.py), outlined like her frames, the set cropped to the union of
its frames, the pivot at the bottom middle. The sparrow flies both ways: the sheet's frames face east, west is mirrored.
Only the yard has them (their ids + 100, as her yard sets)."""
import json, os, sys
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import pixellab_to_sprites as ps
import sprite_clean as sc

CELL = 64
# set: (sheet, catalogue id, the sheet px per game px, fps, mirrored west)
SETS = {
    "bird_fly":  ("yard_bird",      59, 2.4, 14, True),     # a sparrow ~17 px long beside her ~70 in the yard
    "butterfly": ("yard_butterfly", 60, 3.2, 12, False),    # a cabbage white ~12 px wide
    "bird_peck": ("yard_bird_peck", 61, 2.4, 8,  True),
}

def frames(sheet):
    im = np.asarray(Image.open(os.path.join(ROOT, "assets", "pixellab", f"{sheet}_sheet.png")).convert("RGBA"))
    n = im.shape[1] // CELL
    return [im[:, k * CELL:(k + 1) * CELL] for k in range(1, n)]          # the first cell is the empty canvas

def build(name, ctx):
    sheet, aid, scale, fps, mirror = SETS[name]
    full, _, pal_lab = ctx
    allowed = np.r_[np.arange(ps.SLOT, ps.SLOT + 40), np.arange(ps.TOY_SLOT, ps.TOY_SLOT + ps.TOY_N)]
    out = []
    for f in frames(sheet):
        idx = sc.resample(f, scale, pal_lab, allowed)
        idx = sc.drop_specks(idx, 3)
        out.append(sc.clean_outline(idx, pal_lab, ps.SLOT + 4))
    h = max(i.shape[0] for i in out); w = max(i.shape[1] for i in out)
    out = [np.pad(i, ((0, h - i.shape[0]), (0, w - i.shape[1]))) for i in out]
    ys, xs = np.nonzero(np.any(np.stack(out), axis=0))
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    ww = x1 - x0 + (x1 - x0) % 2
    out = [np.pad(i[y0:y1, x0:x1], ((0, 0), (0, ww - (x1 - x0)))) for i in out]
    seqs = {6: out, 2: [i[:, ::-1].copy() for i in out]} if mirror else {0: out}
    return seqs, (int(ww), int(y1 - y0)), (int(ww // 2), int(y1 - y0)), aid, fps

def preview(sets, full):
    pal = np.array(full, np.uint8); rows = []
    for seqs in sets:
        tiles = []
        for idx in seqs[sorted(seqs)[-1]]:
            rgb = np.zeros(idx.shape + (4,), np.uint8); rgb[..., :3] = pal[idx]; rgb[..., 3] = np.where(idx > 0, 255, 0)
            im = Image.fromarray(rgb, "RGBA"); tiles.append(im.resize((im.width * 4, im.height * 4), Image.NEAREST))
        rows.append(tiles)
    W = max(sum(t.width + 8 for t in r) for r in rows) + 8; H = sum(max(t.height for t in r) + 8 for r in rows) + 8
    out = Image.new("RGBA", (W, H), (96, 140, 70, 255)); y = 8
    for r in rows:
        x = 8
        for t in r: out.alpha_composite(t, (x, y)); x += t.width + 8
        y += max(t.height for t in r) + 8
    path = os.path.join(ROOT, "docs", "img", "critters.png"); out.save(path); return path

def main():
    ctx = ps.context(); full = ctx[0]
    names = [n for n in SETS if os.path.exists(os.path.join(ROOT, "assets", "pixellab", f"{SETS[n][0]}_sheet.png"))]
    built = {n: build(n, ctx) for n in names}
    print("preview:", preview([built[n][0] for n in names], full))
    if "--preview" in sys.argv: return
    p = os.path.join(ROOT, "assets", "anims.json")
    cat = json.load(open(p, encoding="utf-8"))
    for n in names:
        seqs, size, piv, aid, fps = built[n]
        ps.write_set(os.path.join(ROOT, "assets", "sprites", "lili_yard", n), n, aid + ps.SIZES[1][2], fps, True, seqs, size, piv, full,
                     {"S": f"assets/pixellab/{SETS[n][0]}_sheet.png"})
        cat[n] = {"id": aid, "frames": len(seqs[sorted(seqs)[0]]), "fps": fps, "dirs": [ps.DIRS[d] for d in sorted(seqs)], "loop": True, "extra": True}
        print(f"{n}: {len(seqs[sorted(seqs)[0]])} frames, {size[0]} x {size[1]} px, id {aid} (+100 in the yard)")
    cat = {k: cat[k] for k in ["_note"] + sorted((k for k in cat if k != "_note"), key=lambda k: cat[k]["id"])}
    json.dump(cat, open(p, "w", encoding="utf-8"), indent=1)

if __name__ == "__main__":
    main()
