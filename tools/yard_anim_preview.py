#!/usr/bin/env python3
"""Pixel Lili: the yard with moving parts, as a preview GIF (2026-10-26, the owner's idea: not a still picture).

    py -3.12 tools/yard_anim_preview.py        assets/pixellab/yard_*_sheet.png -> docs/img/anim/yard_preview_summer.gif

PixelLab trials, nothing of it is in the game: the left tree's crown sways in the wind (PixMiniMax, the
cover layer as the first frame), the right bush rustles when Lili pushes into it (played once, then still),
sparrows fly across the sky and a butterfly flutters over the flower bed (Create animated object, 64 px,
16 frames, scaled down to the yard's size). The tree and the bush sheets were made from the cover layers
padded into a bigger cell (tools: the PAD table says where the layer sat), so a frame goes back to the cover's
own place in the 480 x 480 yard.
"""
import json, os
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PX = os.path.join(ROOT, "assets", "pixellab")
GAME = os.path.join(ROOT, "assets", "backgrounds", "game")
# sheet tag -> (cover layer, its padding inside the PixelLab cell: x, y)
PAD = {"yard_tree_summer": ("cover_summer_left_tree", (7, 0)), "yard_bush_summer": ("cover_summer_right_bush", (9, 25))}
FRAMES, MS = 64, 90                        # the GIF: 64 frames at 90 ms (~5.8 s loop)

CELL_W = {"yard_tree_summer": 160}            # the cell width when it is not the sheet's height

def sheet(tag):
    """the sheet's frames, empty ones (the editor's placeholders) left out; frame 0 is the start frame"""
    s = Image.open(os.path.join(PX, tag + "_sheet.png")).convert("RGBA")
    w = CELL_W.get(tag, s.height)
    cells = [s.crop((i * w, 0, (i + 1) * w, s.height)) for i in range(s.width // w)]
    return [c for c in cells if np.asarray(c)[..., 3].any()]

def place(tag):
    layer, (px, py) = PAD[tag]
    for im in json.load(open(os.path.join(GAME, "game.json"), encoding="utf-8"))["images"]:
        if im["file"] == layer + ".png": return im["x"] - px, im["y"] - py
    raise KeyError(layer)

def small(frames, size):
    """a 64 px sprite down to the yard's scale: nearest, on its own bounding box"""
    out = []
    for f in frames:
        bb = f.getbbox()
        c = f.crop(bb) if bb else f
        s = size / max(c.size)
        out.append(c.resize((max(1, round(c.width * s)), max(1, round(c.height * s))), Image.NEAREST))
    return out

def main():
    yard = Image.open(os.path.join(GAME, "yard_summer.png")).convert("RGBA")
    tree, bush = sheet("yard_tree_summer"), sheet("yard_bush_summer")
    tx, ty = place("yard_tree_summer"); bx, by = place("yard_bush_summer")
    birds = small(sheet("yard_bird"), 22) if os.path.exists(os.path.join(PX, "yard_bird_sheet.png")) else []
    fly = small(sheet("yard_butterfly"), 14) if os.path.exists(os.path.join(PX, "yard_butterfly_sheet.png")) else []
    out = []
    for t in range(FRAMES):
        f = yard.copy()
        f.alpha_composite(tree[1 + (t // 2) % (len(tree) - 1)], (tx, ty))              # the crown sways, 2 GIF frames each
        k = t - 24                                                                       # Lili pushes into the bush at t = 24
        f.alpha_composite(bush[1 + k] if 0 <= k < len(bush) - 1 else bush[0], (bx, by))
        for i, (x0, y0, v) in enumerate(((500, 26, -8), (640, 48, -7))):                 # two sparrows crossing the sky, facing left
            if birds:
                b = birds[(t + 5 * i) % len(birds)]
                x = x0 + v * t; y = y0 + int(4 * np.sin(t / 3 + i))
                if 0 <= x <= yard.width - b.width: f.alpha_composite(b, (x, y))
                elif -b.width < x < 0: f.alpha_composite(b.crop((-x, 0, b.width, b.height)), (0, y))
                elif yard.width - b.width < x < yard.width: f.alpha_composite(b.crop((0, 0, yard.width - x, b.height)), (x, y))
        if fly:
            b = fly[t % len(fly)]
            f.alpha_composite(b, (70 + int(30 * np.sin(t / 9)), 222 + int(10 * np.sin(t / 4))))
        out.append(f.convert("RGB"))
    dst = os.path.join(ROOT, "docs", "img", "anim", "yard_preview_summer.gif")
    out[0].save(dst, save_all=True, append_images=out[1:], duration=MS, loop=0)
    print(f"-> {dst} ({len(tree) - 1} tree, {len(bush) - 1} bush, {len(birds)} bird, {len(fly)} butterfly frames)")
    # each part alone, 2x on the yard's grass, for the review page
    for tag in ("yard_tree_summer", "yard_bush_summer", "yard_bird", "yard_butterfly"):
        if not os.path.exists(os.path.join(PX, tag + "_sheet.png")): continue
        fr = sheet(tag); fr = fr[1:] if tag in PAD else fr
        w, h = fr[0].size; s = 2 if max(w, h) > 100 else 3
        gif = []
        for f in fr:
            b = Image.new("RGBA", (w, h), (108, 150, 70, 255)); b.alpha_composite(f)
            gif.append(b.convert("RGB").resize((w * s, h * s), Image.NEAREST))
        gif[0].save(os.path.join(ROOT, "docs", "img", "anim", tag + ".gif"), save_all=True, append_images=gif[1:],
                    duration=120 if tag in PAD else 80, loop=0)

if __name__ == "__main__":
    main()
