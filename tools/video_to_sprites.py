#!/usr/bin/env python3
"""Pixel Lili: generated frames (FLUX.2 klein keyframes, tools/keyframes.py) back onto Lili's 96 px grid (2026-10-23).
(Written for the Wan video model test, which was dropped and deleted the same day: docs/decisions.md.)

    py -3.12 tools/video_to_sprites.py --tag walk_SE --anim walk --dir SE [--n 6 --fps 10]

assets/raw_renders/video/<tag>/f*.png (the sprite on green, scale.txt says how big) ->
  1. the green screen keyed out (green clearly above red and blue), the result resampled back onto the 96 grid (scale.txt)
     with the same rules as tools/sprite_clean.py (mode colour, > 50 % coverage, Lili's 40 colours, outline)
  2. every frame aligned to frame 0 by the paws (the bottom row and its centre), so the model's drift does
     not make her slide
  3. a loop: the frame most like frame 0 after a minimum length closes it; n frames sampled evenly before it
  4. written as assets/sprites/lili/<anim>/d<dir>_f<i>.png (other directions kept) + the manifest updated,
     and docs/img/anim/<anim>_video.gif to look at
"""
import argparse, glob, json, os, sys
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from palette_from_photos import rgb_to_oklab
import sprite_clean as sc
import animate2d as an

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tag", required=True); ap.add_argument("--anim", required=True); ap.add_argument("--dir", required=True)
    ap.add_argument("--n", type=int, default=6); ap.add_argument("--fps", type=int, default=10); ap.add_argument("--min-loop", type=int, default=8)
    ap.add_argument("--preview", action="store_true", help="only docs/img/anim/<tag>.gif: a candidate, the game's folder untouched")
    ap.add_argument("--ref", help="the source pose (lili_src): frames are aligned to ITS paws, and --pin copies from it")
    ap.add_argument("--pin", help="x0,y0,x1,y1[;...] boxes copied from --ref into every frame (the head: her face and the folded ear never change)")
    a = ap.parse_args()
    raw = sorted(glob.glob(os.path.join(ROOT, "assets", "raw_renders", "video", a.tag, "f*.png")))
    ref = Image.open(os.path.join(ROOT, "assets", "sprites", "lili_src", "stand_S.png"))
    pal = np.array(ref.getpalette()[:768], dtype=np.uint8).reshape(256, 3)
    pal_lab = rgb_to_oklab(pal)
    slots = np.arange(sc.SPRITE_SLOT, sc.SPRITE_SLOT + sc.SPRITE_K)
    # the outline colour the cleaned sprites use: the most common index on a source sprite's silhouette edge
    first = np.asarray(ref); body = first != 0; pad = np.pad(body, 1)
    edge = body & ~(pad[:-2, 1:-1] & pad[2:, 1:-1] & pad[1:-1, :-2] & pad[1:-1, 2:])
    outline = int(np.bincount(first[edge]).argmax())
    sf = os.path.join(ROOT, "assets", "raw_renders", "video", a.tag, "scale.txt")
    scale = float(open(sf).read()) if os.path.exists(sf) else 4.0
    out_frames = []
    for f in raw:
        rgb = np.asarray(Image.open(f).convert("RGB")).astype(int)
        green = (rgb[..., 1] > rgb[..., 0] + 40) & (rgb[..., 1] > rgb[..., 2] + 40)
        rgba = np.dstack([rgb.astype(np.uint8), np.where(green, 0, 255).astype(np.uint8)])
        img = sc.resample(rgba, scale, pal_lab, slots)
        img = sc.drop_specks(img)
        img = sc.clean_outline(img, pal_lab, outline)
        out_frames.append(img)
    # align by the paws: the bottom row and the centre of the bottom 6 rows
    def foot(img):
        ys, xs = np.nonzero(img)
        if not len(ys): return None
        b = ys.max(); sel = ys >= b - 5
        return b, int(round(xs[sel].mean()))
    refimg = np.asarray(Image.open(os.path.join(ROOT, "assets", "sprites", "lili_src", a.ref + ".png"))) if a.ref else None
    b0, c0 = foot(refimg if refimg is not None else out_frames[0])
    aligned = []
    for img in out_frames:
        fb = foot(img)
        if fb is None: aligned.append(img); continue
        dy, dx = b0 - fb[0], c0 - fb[1]
        sh = np.zeros_like(img)
        ys, xs = np.nonzero(img)
        ny, nx = ys + dy, xs + dx
        ok = (ny >= 0) & (ny < img.shape[0]) & (nx >= 0) & (nx < img.shape[1])
        sh[ny[ok], nx[ok]] = img[ys[ok], xs[ok]]
        aligned.append(sh)
    if a.pin and refimg is not None:                 # the ears from the approved sprite: the model only moves the body
        boxes = [tuple(map(int, box.split(","))) for box in a.pin.split(";")]
        def head_fit(fr):
            """re-align a frame to the reference by the pinned boxes' surroundings (the head), not the paws: the
            model keeps the head in place, and a lifted paw would shift the whole frame"""
            best = (10**9, 0, 0)
            for dy in range(-6, 7):
                for dx in range(-6, 7):
                    sh = np.roll(np.roll(fr, dy, 0), dx, 1)
                    err = 0
                    for x0, y0, x1, y1 in boxes:
                        X0, Y0, X1, Y1 = max(0, x0 - 6), max(0, y0), min(96, x1 + 10), min(96, y1 + 14)
                        err += ((sh[Y0:Y1, X0:X1] != 0) != (refimg[Y0:Y1, X0:X1] != 0)).sum()
                    if err < best[0]: best = (err, dy, dx)
            return np.roll(np.roll(fr, best[1], 0), best[2], 1)
        aligned = [head_fit(fr) for fr in out_frames]
        for fr in aligned:
            for box in a.pin.split(";"):
                x0, y0, x1, y1 = map(int, box.split(","))
                fr[y0:y1, x0:x1] = refimg[y0:y1, x0:x1]
        aligned = [sc.clean_outline(sc.drop_specks(fr), pal_lab, outline) for fr in aligned]
    # the loop: the frame most like frame 0 after min-loop frames
    def diff(p, q): return ((p != 0) != (q != 0)).sum() + (p != q).sum() * 0.25
    cands = range(a.min_loop, len(aligned))
    end = min(cands, key=lambda k: diff(aligned[0], aligned[k])) if len(aligned) > a.min_loop else len(aligned)
    picks = [int(round(i * end / a.n)) for i in range(a.n)]
    seq = [aligned[i] for i in picks]
    if a.preview:
        an.gif(os.path.join(ROOT, "docs", "img", "anim", f"{a.tag}.gif"), {a.dir: seq}, pal, a.fps)
        an.gif(os.path.join(ROOT, "docs", "img", "anim", f"{a.tag}_raw.gif"), {a.dir: aligned}, pal, 16)
        print(f"{a.tag}: {len(raw)} frames, loop at {end}, picked {picks} -> docs/img/anim/{a.tag}.gif (preview)")
        return
    print(f"{len(raw)} frames, loop closes at {end} (diff {diff(aligned[0], aligned[end]) if end < len(aligned) else 0:.0f}), picked {picks}")
    # write into the animation folder, other directions kept
    di = an.DIRS.index(a.dir)
    d = os.path.join(ROOT, "assets", "sprites", "lili", a.anim); os.makedirs(d, exist_ok=True)
    mp = os.path.join(d, "manifest.json")
    man = json.load(open(mp, encoding="utf-8")) if os.path.exists(mp) else {"anim": a.anim, "id": 0, "fps": a.fps, "loop": True, "dirs": [], "frames": a.n,
                                                                           "cell": an.CELL, "scale": 1, "pivot": list(an.PIVOT), "files": []}
    if man["frames"] != a.n:                         # a different frame count: this direction alone, the others dropped
        for f in man["files"]:
            p = os.path.join(d, f["file"])
            if os.path.exists(p): os.remove(p)
        man["files"], man["dirs"] = [], []
    man["frames"], man["fps"] = a.n, a.fps
    man["files"] = [f for f in man["files"] if f["dir"] != di]
    for i, fr in enumerate(seq):
        fn = f"d{di}_f{i}.png"
        an.save_indexed(os.path.join(d, fn), fr, pal)
        man["files"].append({"dir": di, "frame": i, "file": fn, "mouth": None, "head": None})
    man["dirs"] = sorted(set(man["dirs"]) | {di})
    man["source"] = "tools/video_to_sprites.py (" + a.tag + ")"
    json.dump(man, open(mp, "w", encoding="utf-8"), indent=1)
    an.gif(os.path.join(ROOT, "docs", "img", "anim", f"{a.anim}_video.gif"), {a.dir: seq}, pal, a.fps)
    an.gif(os.path.join(ROOT, "docs", "img", "anim", f"{a.tag}_raw.gif"), {a.dir: aligned}, pal, 16)
    print(f"-> {os.path.relpath(d, ROOT)} ({a.n} frames, dir {a.dir}), docs/img/anim/{a.anim}_video.gif")

if __name__ == "__main__":
    main()
