#!/usr/bin/env python3
"""Pixel Lili: B7, the pixelizer (spec 7.5). 4x renders -> 8-bit indexed sprites on the master palette.

    py -3.12 tools/pixelize.py [--anim walk] [--raw assets/raw_renders] [--out assets/sprites/lili]

The rules, as the spec lists them:
  1. every 4 x 4 block becomes one pixel whose colour is the block's MOST COMMON palette colour
     (each covered source pixel is first snapped to the nearest master colour in Oklab), not an
     average: averages smear
  2. alpha: a block is opaque if more than half of it is covered, else transparent (index 0);
     no half-transparent pixels on a sprite
  3. the palette mapping has no dithering
  4. a 1 px outline around the outside of the silhouette, 8-neighbourhood: a SELECTIVE outline
     (spec 7.1, 2026-10-21): the darkest shade of the ramp of the body pixel it touches (dark fur
     next to fur, dark white next to the chest); next to a fixed-slot colour (eye, nose) PAL_OUTLINE
  5. a frame that exists in assets/sprites/overrides/<anim>/ (hand-fixed) is copied, never overwritten
  6. no baked shadow: the game draws the ground ellipse (so it is right on the rug and the bench)
Output: indexed PNGs (the master palette embedded), manifest.json copied with the cell data, and a
contact sheet per animation in docs/img/sheets/ (the approval gate, B8).
"""
import argparse, json, os, shutil, subprocess, sys
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from palette_from_photos import rgb_to_oklab          # the one Oklab

PAL_CLEAR, PAL_OUTLINE = 0, 1
NEVER = {0, 4, 5, 6, 14, 15}                          # transparent, debug, UI, spare: never a sprite colour

def load_palette():
    m = json.load(open(os.path.join(ROOT, "assets", "palettes", "master.json"), encoding="utf-8"))
    rgb = np.array(m["rgb"], dtype=np.uint8)
    lab = rgb_to_oklab(rgb)
    ok = np.array([i not in NEVER for i in range(256)])
    return rgb, lab, ok

def nearest(lab_px, pal_lab, ok):
    """(n, 3) Oklab -> (n,) palette index, among the allowed slots"""
    d = ((lab_px[:, None, :] - pal_lab[None, :, :]) ** 2).sum(2)
    d[:, ~ok] = 1e9
    return d.argmin(1)

def pixelize(png, pal_rgb, pal_lab, ok, scale):
    im = np.asarray(Image.open(png).convert("RGBA"))
    H, W = im.shape[0] // scale, im.shape[1] // scale
    covered = im[..., 3] > 127                       # straight alpha from Blender; 50 % is the cut
    cover = covered.reshape(H, scale, W, scale).mean(axis=(1, 3))
    out = np.zeros((H, W), dtype=np.uint8)
    # snap every covered source pixel to the palette once, then take the mode per block
    idx_full = np.zeros(im.shape[:2], dtype=np.int32)
    if covered.any():
        idx_full[covered] = nearest(rgb_to_oklab(im[..., :3][covered]), pal_lab, ok)
    # the mode per block, vectorised: one-hot counts of the covered source pixels (blocks x 256)
    idx_blk = idx_full.reshape(H, scale, W, scale).transpose(0, 2, 1, 3).reshape(H * W, scale * scale)
    cov_blk = covered.reshape(H, scale, W, scale).transpose(0, 2, 1, 3).reshape(H * W, scale * scale)
    counts = np.zeros((H * W, 256), dtype=np.int32)
    rows = np.repeat(np.arange(H * W), scale * scale).reshape(H * W, scale * scale)
    np.add.at(counts, (rows[cov_blk], idx_blk[cov_blk]), 1)
    mode = counts.argmax(1).reshape(H, W).astype(np.uint8)
    out[cover > 0.5] = mode[cover > 0.5]
    # the outline: outside the silhouette, 8-neighbourhood, selective (the neighbour's ramp, darkest shade)
    body = out != 0
    neighbour = np.zeros((H, W), dtype=np.uint8)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            if dy or dx:
                src = body[max(0, -dy):H - max(0, dy), max(0, -dx):W - max(0, dx)]
                val = out[max(0, -dy):H - max(0, dy), max(0, -dx):W - max(0, dx)]
                tgt = neighbour[max(0, dy):H + min(0, dy), max(0, dx):W + min(0, dx)]
                np.copyto(tgt, val, where=src & (tgt == 0))
    rim = (neighbour != 0) & ~body
    ramp_dark = np.where(neighbour >= 16, 16 + ((neighbour - 16) // 8) * 8, PAL_OUTLINE).astype(np.uint8)
    out[rim] = ramp_dark[rim]
    return out

def save_indexed(path, idx, pal_rgb):
    im = Image.fromarray(idx, mode="P")
    im.putpalette(pal_rgb.flatten().tolist())
    im.info["transparency"] = 0
    im.save(path, transparency=0)

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--anim", default="all")
    ap.add_argument("--raw", default="assets/raw_renders")
    ap.add_argument("--out", default="assets/sprites/lili")
    a = ap.parse_args()
    pal_rgb, pal_lab, ok = load_palette()
    raw = os.path.join(ROOT, a.raw)
    anims = [d for d in sorted(os.listdir(raw)) if os.path.exists(os.path.join(raw, d, "manifest.json"))] if a.anim == "all" else [a.anim]
    for name in anims:
        man = json.load(open(os.path.join(raw, name, "manifest.json"), encoding="utf-8"))
        out = os.path.join(ROOT, a.out, name)
        os.makedirs(out, exist_ok=True)
        over = os.path.join(ROOT, "assets", "sprites", "overrides", name)
        n_over = 0
        for fr in man["files"]:
            dst = os.path.join(out, fr["file"])
            src_over = os.path.join(over, fr["file"])
            if os.path.exists(src_over):
                shutil.copyfile(src_over, dst); n_over += 1
                continue
            idx = pixelize(os.path.join(raw, name, fr["file"]), pal_rgb, pal_lab, ok, man["scale"])
            assert idx.shape == (man["cell"], man["cell"]), idx.shape
            save_indexed(dst, idx, pal_rgb)
        json.dump(man, open(os.path.join(out, "manifest.json"), "w", encoding="utf-8"), indent=1)
        sheet = os.path.join(ROOT, "docs", "img", "sheets", f"{name}.png")
        subprocess.check_call([sys.executable, os.path.join(ROOT, "tools", "contact_sheet.py"), out, sheet, "--scale", "3", "--cols", str(man["frames"])])
        print(f"pixelize: {name}: {len(man['files'])} frames ({n_over} hand-fixed overrides) -> {os.path.relpath(out, ROOT)}")

if __name__ == "__main__":
    main()
