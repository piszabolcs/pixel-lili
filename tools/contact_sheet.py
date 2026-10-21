#!/usr/bin/env python3
"""Pixel Lili: put rendered or pixelized frames side by side (B8, spec 7.5), for a look or an approval.

    py -3.12 tools/contact_sheet.py assets/raw_renders/_check docs/img/m3_1_check.png
    py -3.12 tools/contact_sheet.py <dir> <out.png> --scale 3 --cols 8

Every PNG in <dir> (sorted by name) becomes one tile on the floor colour, labelled with
its name without the extension; nearest-neighbour scaling only, so a pixel stays a pixel.
A thin line marks the pivot row (logical y 40 of a 48 px cell = 5/6 of the tile height).
"""
import argparse, glob, os
from PIL import Image, ImageDraw

FLOOR = (236, 222, 200)
INK = (59, 36, 24)

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("src")
    ap.add_argument("out")
    ap.add_argument("--scale", type=int, default=1)
    ap.add_argument("--cols", type=int, default=8)
    ap.add_argument("--no-pivot", action="store_true")
    a = ap.parse_args()
    files = sorted(glob.glob(os.path.join(a.src, "*.png")))
    if not files:
        raise SystemExit(f"no PNG in {a.src}")
    tiles = [Image.open(f).convert("RGBA") for f in files]
    tw, th = (tiles[0].width * a.scale, tiles[0].height * a.scale)
    pad, label = 8, 14
    cols = min(a.cols, len(tiles))
    rows = (len(tiles) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * (tw + pad) + pad, rows * (th + pad + label) + pad), FLOOR)
    d = ImageDraw.Draw(sheet)
    for i, (f, t) in enumerate(zip(files, tiles)):
        x = pad + (i % cols) * (tw + pad)
        y = pad + (i // cols) * (th + pad + label)
        if not a.no_pivot:
            py = y + th * 40 // 48
            d.line([(x, py), (x + tw - 1, py)], fill=(222, 206, 182))
        sheet.paste(t.resize((tw, th), Image.NEAREST), (x, y), t.resize((tw, th), Image.NEAREST))
        d.text((x + 2, y + th + 1), os.path.splitext(os.path.basename(f))[0], fill=INK)
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    sheet.save(a.out)
    print(f"contact sheet: {len(tiles)} tiles -> {a.out} ({sheet.width} x {sheet.height})")

if __name__ == "__main__":
    main()
