#!/usr/bin/env python3
"""Pixel Lili: 2D puppet animation on the cleaned sprites (2026-10-23, the 2D line, round 1).

    py -3.12 tools/animate2d.py            assets/sprites/lili_src/*.png -> assets/sprites/lili/<anim>/ + docs/img/anim/<anim>.gif

Only motions that need no hand-drawn masks, so nothing tears:
  breathing  everything above a chest line moves up by 0-1 px, the line's row stretches into the gap,
             the paws never move (the line: 45 % down the silhouette)
  blink      each eye (found as an interior dark blob, not part of the outline) is painted over with the
             fur around it and a 1 px closed lid is drawn across its lower third
Tail wags and the walk cycle come from the video model (tools/video_anim.py), not from here.
The frames go where tools/pack_assets.py reads them: d<dir>_f<frame>.png + manifest.json per animation.
"""
import json, os, sys
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from palette_from_photos import rgb_to_oklab

SRC = os.path.join(ROOT, "assets", "sprites", "lili_src")
OUT = os.path.join(ROOT, "assets", "sprites", "lili")
DIRS = ("S", "SW", "W", "NW", "N", "NE", "E", "SE")              # common/lili/lili_types.h dir8_t
CELL, PIVOT = 96, (48, 90)

# animation -> (catalogue id, fps, loop, {direction: pose}, per-frame breath offsets, blink frames)
ANIMS = {
    "idle_stand":     (1, 6, True, {"S": "stand_S", "SW": "stand_SW", "W": "stand_W", "N": "stand_N", "NE": "stand_NE", "SE": "stand_SE"},
                       [0, 0, 1, 1, 1, 0], []),
    "idle_sit_blink": (2, 6, True, {"S": "sit_S"}, [0, 0, 1, 1, 1, 0, 0, 0], [5, 6]),
    "pant_smile":     (9, 8, True, {"SW": "happy_SW", "S": "happy_sit_S"}, [0, 1, 0, 1, 0, 1], []),
    "sleep_curl":     (18, 3, True, {"SW": "sleep"}, [0, 1, 1, 0], []),
    "walk":           (4, 10, True, {"SE": "walk_SE"}, [0, 1, 0, 0, 1, 0], []),     # a bob only: the stride comes from the video model
}

def load(pose):
    im = Image.open(os.path.join(SRC, pose + ".png"))
    return np.asarray(im).copy(), np.array(im.getpalette()[:768], dtype=np.uint8).reshape(256, 3)

def breathe(img, off, line):
    if not off: return img.copy()
    out = img.copy()
    out[0:line - off] = img[off:line]                 # the upper body up by `off`
    out[line - off:line] = img[line - 1]              # the chest row stretches into the gap
    return out

def find_eyes(img, L):
    body = img != 0
    pad = np.pad(body, 1)
    inner = pad[1:-1, 1:-1] & ~(pad[:-2, 1:-1] & pad[2:, 1:-1] & pad[1:-1, :-2] & pad[1:-1, 2:])
    dark = body & (L[img] < 0.30) & ~inner
    H, W = img.shape; seen = np.zeros_like(dark); blobs = []
    for y0, x0 in zip(*np.nonzero(dark)):
        if seen[y0, x0]: continue
        st, c = [(y0, x0)], []; seen[y0, x0] = True
        while st:
            y, x = st.pop(); c.append((y, x))
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < H and 0 <= xx < W and dark[yy, xx] and not seen[yy, xx]:
                        seen[yy, xx] = True; st.append((yy, xx))
        if len(c) >= 12: blobs.append(c)
    if not blobs: return []
    top = min(min(p[0] for p in b) for b in blobs)
    return [b for b in blobs if min(p[0] for p in b) <= top + 4][:2]     # the upper pair (a mouth sits lower)

def blink(img, eyes, L, outline):
    out = img.copy()
    for b in eyes:
        ys = [p[0] for p in b]; xs = [p[1] for p in b]
        y0, y1, x0, x1 = min(ys) - 1, max(ys) + 2, min(xs) - 1, max(xs) + 2
        ring = []
        for y in range(y0 - 1, y1 + 1):
            for x in range(x0 - 1, x1 + 1):
                if (y in (y0 - 1, y1) or x in (x0 - 1, x1)) and out[y, x] != 0 and L[out[y, x]] > 0.45:
                    ring.append(out[y, x])
        fur = max(set(ring), key=ring.count) if ring else img[y0 - 2, (x0 + x1) // 2]
        for y in range(y0, y1):
            for x in range(x0, x1):
                if out[y, x] != 0: out[y, x] = fur
        lid = y0 + (y1 - y0) * 2 // 3
        out[lid, x0 + 1:x1 - 1] = outline                                 # the closed lid, a slight smile of a line
        out[lid - 1, x0] = outline; out[lid - 1, x1 - 1] = outline
    return out

def save_indexed(path, idx, pal):
    im = Image.fromarray(idx, mode="P"); im.putpalette(pal.flatten().tolist()); im.save(path, transparency=0)

def gif(path, frames_by_dir, pal, fps, scale=4):
    """all directions side by side, on the floor colour, nearest x scale"""
    dirs = list(frames_by_dir)
    n = len(frames_by_dir[dirs[0]])
    W = CELL * scale
    shots = []
    for f in range(n):
        canvas = Image.new("RGB", (len(dirs) * W, W), (210, 160, 120))
        for k, d in enumerate(dirs):
            fr = frames_by_dir[d][f]
            im = Image.fromarray(fr, "P"); im.putpalette(pal.flatten().tolist())
            rgba = np.asarray(im.convert("RGBA")).copy(); rgba[..., 3] = np.where(fr != 0, 255, 0)
            sp = Image.fromarray(rgba).resize((W, W), Image.NEAREST)
            canvas.paste(sp, (k * W, 0), sp)
        shots.append(canvas)
    shots[0].save(path, save_all=True, append_images=shots[1:], duration=int(1000 / fps), loop=0, disposal=2)

def main():
    os.makedirs(os.path.join(ROOT, "docs", "img", "anim"), exist_ok=True)
    for name, (aid, fps, loop, by_dir, breath, blinks) in ANIMS.items():
        out = os.path.join(OUT, name)
        os.makedirs(out, exist_ok=True)
        for f in os.listdir(out):
            if f.endswith(".png"): os.remove(os.path.join(out, f))
        files, frames_by_dir, pal = [], {}, None
        dirs = sorted(DIRS.index(d) for d in by_dir)
        for di in dirs:
            img, pal = load(by_dir[DIRS[di]])
            L = rgb_to_oklab(pal)[:, 0]
            outline = int(np.bincount(img[(img != 0) & (L[img] < 0.2)]).argmax()) if ((img != 0) & (L[img] < 0.2)).any() else 1
            ys = np.nonzero((img != 0).any(1))[0]
            line = int(ys.min() + 0.45 * (ys.max() - ys.min()))
            eyes = find_eyes(img, L) if blinks else []
            seq = []
            for fi, off in enumerate(breath):
                fr = breathe(img, off, line)
                if fi in blinks and eyes: fr = blink(fr, eyes, L, outline)
                fn = f"d{di}_f{fi}.png"
                save_indexed(os.path.join(out, fn), fr, pal)
                files.append({"dir": di, "frame": fi, "file": fn, "mouth": None, "head": None})
                seq.append(fr)
            frames_by_dir[DIRS[di]] = seq
        json.dump({"anim": name, "id": aid, "fps": fps, "loop": loop, "dirs": dirs, "frames": len(breath),
                   "cell": CELL, "scale": 1, "pivot": list(PIVOT), "files": files, "source": "tools/animate2d.py"},
                  open(os.path.join(out, "manifest.json"), "w", encoding="utf-8"), indent=1)
        gif(os.path.join(ROOT, "docs", "img", "anim", name + ".gif"), frames_by_dir, pal, fps)
        print(f"{name:<15} id {aid:>2}: {len(dirs)} dirs x {len(breath)} frames @ {fps} fps -> {os.path.relpath(out, ROOT)}")

if __name__ == "__main__":
    main()
