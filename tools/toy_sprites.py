#!/usr/bin/env python3
"""Pixel Lili: the rope toy's sprites from the owner's Gemini props sheet (2026-10-27, the owner: the toy on the floor
must be the generated one, not the code-drawn bars).

    py -3.12 tools/toy_sprites.py           assets/sprites/{lili,lili_yard}/toy_rope, lili/icon_rope, anims.json, docs/img/toys.png
    py -3.12 tools/toy_sprites.py --preview docs/img/toys.png only

docs/art/Lilik/07.jpg (docs/art/startframes/PROMPTS.md 07) holds the knotted rope with frayed ends in 4 rotations on
magenta. Each is keyed out (tools/startframes_cut.py's magenta test), resampled onto the game's pixel grid (each output
pixel the most common colour under it, tools/sprite_clean.py; the rotations that lie into the room foreshortened, DEPTH)
in Lili's 40 colours and the toys' 6
(assets/palettes/toy_colors.json, the rope's own blues, reds and yellows), and outlined like her frames. The sizes are
the rope in her mouth (tools/pixellab_to_sprites.py carry_trot S: 44 px wide in the room, 36 in the yard), so it is the
same toy on the floor and in her mouth. The 4 rotations are the set's 4 "directions" (0 lying across, 1 rising to the
right, 2 upright, 3 falling to the right); the game turns it as it flies and lets it land any way (lili_world.c).
"""
import json, os, sys
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import pixellab_to_sprites as ps
import sprite_clean as sc
import startframes_cut as sfc
from palette_from_photos import rgb_to_oklab

SHEET = os.path.join(ROOT, "docs", "art", "Lilik", "07.jpg")
ROWS = (0, 300)                              # the ropes are the sheet's top row, the biscuits right of them
ROOM_W, YARD_W, ICON_W = 46, 38, 22          # the lying-across rope's width: as in her mouth; the bubble's
TOYS = {"toy_rope": 40, "icon_rope": 42}     # catalogue ids (anims.json "extra"); the yard's set is id + 100
DEPTH = (1.0, 0.85, 0.7, 0.85)               # the floor seen from above at a slant: a rope lying into the room looks shorter

def ropes():
    """the 4 rotations, RGBA crops of the sheet, left to right"""
    rgb = np.asarray(Image.open(SHEET).convert("RGB"))[ROWS[0]:ROWS[1]]
    alpha = ~sfc.key_magenta(rgb)
    boxes = sorted(sfc.pieces(alpha), key=lambda b: b[0])
    boxes = [b for b in boxes if (b[2] - b[0]) * (b[3] - b[1]) > 4000][:4]       # the biscuits are small
    out = []
    for x0, y0, x1, y1 in boxes:
        a = np.zeros((y1 - y0, x1 - x0, 4), np.uint8)
        a[..., :3] = rgb[y0:y1, x0:x1]; a[..., 3] = np.where(alpha[y0:y1, x0:x1], 255, 0)
        out.append(a)
    return out

def sized(rgba, cell, ctx, depth=1.0):
    full, _, pal_lab = ctx
    if depth != 1.0:
        im = Image.fromarray(rgba, "RGBA"); rgba = np.asarray(im.resize((im.width, round(im.height * depth)), Image.NEAREST))
    allowed = np.r_[np.arange(ps.SLOT, ps.SLOT + 40), np.arange(ps.TOY_SLOT, ps.TOY_SLOT + ps.TOY_N)]
    idx = sc.resample(rgba, cell, pal_lab, allowed)
    idx = sc.drop_specks(idx, 4)
    idx = sc.clean_outline(idx, pal_lab, ps.SLOT + 4)                # the dark outline her frames use (index 148)
    ys, xs = np.nonzero(idx)
    return idx[ys.min():ys.max() + 1, xs.min():xs.max() + 1]

def cell_set(frames):
    """the rotations in one cell, each centred on the pivot column, standing on the pivot row"""
    w = max(f.shape[1] for f in frames); h = max(f.shape[0] for f in frames)
    w += w % 2
    seqs = {}
    for d, f in enumerate(frames):
        c = np.zeros((h, w), np.uint8)
        ox, oy = (w - f.shape[1]) // 2, h - f.shape[0]
        c[oy:oy + f.shape[0], ox:ox + f.shape[1]] = f
        seqs[d] = [c]
    return seqs, (w, h), (w // 2, h)

def write(sub, name, aid, seqs, size, piv, full):
    ps.write_set(os.path.join(ROOT, "assets", "sprites", sub, name), name, aid, 1, True, seqs, size, piv, full,
                 {"S": "docs/art/Lilik/07.jpg"})

def preview(sets, full):
    pal = np.array(full, np.uint8); tiles = []
    for seqs in sets:
        for d in sorted(seqs):
            idx = seqs[d][0]
            rgb = np.zeros(idx.shape + (4,), np.uint8); rgb[..., :3] = pal[idx]; rgb[..., 3] = np.where(idx > 0, 255, 0)
            im = Image.fromarray(rgb, "RGBA"); tiles.append(im.resize((im.width * 4, im.height * 4), Image.NEAREST))
    W = sum(t.width + 12 for t in tiles) + 12; H = max(t.height for t in tiles) + 24
    out = Image.new("RGBA", (W, H), (150, 104, 80, 255)); x = 12
    for t in tiles: out.alpha_composite(t, (x, 12)); x += t.width + 12
    path = os.path.join(ROOT, "docs", "img", "toys.png"); out.save(path); return path

def main():
    ctx = ps.context(); full = ctx[0]
    rs = ropes()
    assert len(rs) == 4, f"found {len(rs)} ropes on the sheet"
    across = rs[0].shape[1]
    room = cell_set([sized(r, across / ROOM_W, ctx, k) for r, k in zip(rs, DEPTH)])
    yard = cell_set([sized(r, across / YARD_W, ctx, k) for r, k in zip(rs, DEPTH)])
    icon = cell_set([sized(rs[0], across / ICON_W, ctx)])
    print("preview:", preview([room[0], yard[0], icon[0]], full))
    if "--preview" in sys.argv: return
    write("lili", "toy_rope", TOYS["toy_rope"], *room, full)
    write("lili_yard", "toy_rope", TOYS["toy_rope"] + ps.SIZES[1][2], *yard, full)
    write("lili", "icon_rope", TOYS["icon_rope"], *icon, full)
    print(f"toy_rope room {room[1]} yard {yard[1]}, icon_rope {icon[1]}")
    p = os.path.join(ROOT, "assets", "anims.json")
    cat = json.load(open(p, encoding="utf-8"))
    cat["toy_rope"] = {"id": TOYS["toy_rope"], "frames": 1, "fps": 1, "dirs": ["S", "SW", "W", "NW"], "loop": True, "extra": True}
    cat["icon_rope"] = {"id": TOYS["icon_rope"], "frames": 1, "fps": 1, "dirs": ["S"], "loop": True, "extra": True}
    json.dump(cat, open(p, "w", encoding="utf-8"), indent=1)

if __name__ == "__main__":
    main()
