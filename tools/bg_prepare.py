#!/usr/bin/env python3
"""Pixel Lili: the owner's generated backgrounds -> game-ready layers (2026-10-23, preview stage).

    py -3.12 tools/bg_prepare.py

assets/backgrounds/src/*.png (1024 px; the real pixel grid is 400 x 400, measured: a 2.56 px period) ->
  assets/backgrounds/room.png             the room on its 400 grid, the door's 9 glass panes transparent
  assets/backgrounds/door_view_<s>.png    what the panes show: the same season's yard, small and soft, as glass
  assets/backgrounds/props/<id>.png       the pad, the vase and the two bowls cut out (their states change, and
                                          they sort with Lili); the floor under them is filled with its own planks
  assets/backgrounds/yard_<s>.png         the yards on their 400 grid
  docs/img/bg/room_<s>_480.png            preview: 480 x 480 with the display corners and Lili
Nothing here goes into the pak; tools/pack_assets.py is untouched until the owner approves.
"""
import os
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets", "backgrounds", "src")
OUT = os.path.join(ROOT, "assets", "backgrounds")
DOC = os.path.join(ROOT, "docs", "img", "bg")
SEASONS = ("spring", "summer", "autumn", "winter")
GRID, SHOW, CORNER = 400, 480, 72

# the door's glass panes on the 400 grid (inclusive), found from the dark muntin rows and columns
PANE_X = ((168, 184), (189, 204), (209, 225))
PANE_Y = ((37, 48), (52, 63), (68, 79))
WINDOW = (168, 37, 226, 80)                    # x0, y0, x1, y1 (exclusive): the whole glazed area
# changeable things on the room floor: (x0, y0, x1, y1) exclusive, on the 400 grid
PROPS = {"pad": (28, 319, 114, 375), "vase": (176, 307, 233, 376),
         "bowl_food": (295, 346, 330, 377), "bowl_water": (332, 345, 367, 378)}
FLOOR_Y = (140, 386)                           # the floor's rows (below the wall, above the front edge)
# the vase's terracotta is the floor's colour (measured: 20-67 apart), so its body is traced by hand along
# its dark outline (400 grid); the flowers above it are found by colour like the other props
HAND_MASKS = {"vase": [(195, 346), (217, 346), (219, 350), (221, 355), (221, 365), (218, 369), (214, 373),
                       (200, 373), (196, 369), (192, 365), (191, 355), (193, 350)]}
VIEW = (40, 0, 360, 236)                       # the part of a yard seen through the door: fence, trees, grass

def load_grid(name):
    """the centre of every 2.56 px source cell = the original pixel"""
    return Image.open(os.path.join(SRC, name + ".png")).convert("RGB").resize((GRID, GRID), Image.NEAREST)

def is_wood(px):
    r, g, b = px[..., 0].astype(int), px[..., 1].astype(int), px[..., 2].astype(int)
    return (r > g) & (g >= b - 4) & (r - b > 20) & (r < 190)

def wood_runs(col, y_from, y_to):
    """clean wood runs (start, end exclusive) in one column between two rows"""
    w = is_wood(col[y_from:y_to])
    runs, start = [], None
    for i, ok in enumerate(w):
        if ok and start is None: start = i
        if not ok and start is not None: runs.append((y_from + start, y_from + i)); start = None
    if start is not None: runs.append((y_from + start, y_to))
    return runs

def fill_floor(img, box):
    """the floor under an object: the boards run vertically, so every column gets the median colour of its
    own clean wood just above and just below the box, blended top to bottom (the floor darkens towards the
    front). Copying texture rows was tried first: the board ends repeated as a grid of horizontal lines."""
    a = np.asarray(img).astype(float).copy()
    x0, y0, x1, y1 = box
    for x in range(x0, x1):
        col = a[:, x]
        above = [col[y] for y in range(max(FLOOR_Y[0], y0 - 30), y0) if is_wood(col[y:y + 1].astype(np.uint8))[0]]
        below = [col[y] for y in range(y1, min(FLOOR_Y[1], y1 + 12)) if is_wood(col[y:y + 1].astype(np.uint8))[0]]
        if not above and not below:
            continue
        ca = np.median(above, 0) if above else np.median(below, 0)
        cb = np.median(below, 0) if below else ca
        for y in range(y0, y1):
            t = (y - y0) / max(1, y1 - y0 - 1)
            a[y, x] = ca * (1 - t) + cb * t
    return Image.fromarray(a.round().clip(0, 255).astype(np.uint8))

def largest_blob(m):
    """the biggest 4-connected True region (the object; thin plank seams that the fill smoothed away are
    separate specks), with its inner holes filled (parts of the object coloured like the floor)"""
    h, w = m.shape
    seen = np.zeros_like(m); best = []
    for sy in range(h):
        for sx in range(w):
            if not m[sy, sx] or seen[sy, sx]: continue
            stack, blob = [(sy, sx)], []
            seen[sy, sx] = True
            while stack:
                y, x = stack.pop(); blob.append((y, x))
                for ny, nx in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
                    if 0 <= ny < h and 0 <= nx < w and m[ny, nx] and not seen[ny, nx]:
                        seen[ny, nx] = True; stack.append((ny, nx))
            if len(blob) > len(best): best = blob
    out = np.zeros_like(m)
    for y, x in best: out[y, x] = True
    # .copy(): an image made by fromarray shares the array's memory and the flood fill left it unchanged
    outside = Image.fromarray(np.pad(~out, 1, constant_values=True).astype(np.uint8) * 255).copy()
    ImageDraw.floodfill(outside, (0, 0), 128)
    holes = np.asarray(outside)[1:-1, 1:-1] == 255
    return out | holes

def cut_props(orig, filled):
    """an object pixel = one that is not wood-coloured, or one far from its board's own colour (the vase's
    terracotta is wood-like but much brighter); the far-from-floor part is opened first so the grain's
    highlights fall away; then the largest region, a 3x3 closing for the notches, and the holes filled.
    A plain difference against the filled floor was tried first: the median fill smooths the grain, so half
    the floor counted as 'changed'. The baked contact shadow stays on the floor; shadows come at runtime."""
    o = np.asarray(orig); f = np.asarray(filled).astype(int)
    def op(m, a, b):
        return np.asarray(Image.fromarray(m.astype(np.uint8) * 255).filter(a(3)).filter(b(3))) > 0
    out = {}
    for pid, (x0, y0, x1, y1) in PROPS.items():
        far = np.abs(o[y0:y1, x0:x1].astype(int) - f[y0:y1, x0:x1]).sum(2) > 120
        m = largest_blob(~is_wood(o[y0:y1, x0:x1]) | op(far, ImageFilter.MinFilter, ImageFilter.MaxFilter))
        if pid in HAND_MASKS:
            hm = Image.new("L", (x1 - x0, y1 - y0), 0)
            ImageDraw.Draw(hm).polygon([(x - x0, y - y0) for x, y in HAND_MASKS[pid]], fill=255, outline=255)
            m = m | (np.asarray(hm) > 0)
        m = largest_blob(op(m, ImageFilter.MaxFilter, ImageFilter.MinFilter))
        rgba = np.dstack([o[y0:y1, x0:x1], (m * 255).astype(np.uint8)])
        out[pid] = (Image.fromarray(rgba, "RGBA"), (x0, y0))
    return out

def door_view(yard):
    """the yard through the glass: the far part of the garden, shrunk to the window, a little lighter and
    cooler like glass; the muntins stay from the room picture"""
    x0, y0, x1, y1 = WINDOW
    v = yard.crop(VIEW).resize((x1 - x0, y1 - y0), Image.BOX)
    a = np.asarray(v).astype(float)
    a = a * 0.86 + np.array([214, 228, 240]) * 0.14
    return Image.fromarray(a.clip(0, 255).astype(np.uint8))

def pane_mask():
    m = np.zeros((GRID, GRID), bool)
    for xa, xb in PANE_X:
        for ya, yb in PANE_Y:
            m[ya:yb + 1, xa:xb + 1] = True
    return m

def show(img400, sprites=(), night=False):
    """the 480 display: 1.2x nearest (every 5th pixel doubled), Lili at native size with her shadow under
    her, rounded corners; night = the evening table: darker and colder (the game swaps the palette LUT)"""
    s = img400.convert("RGB").resize((SHOW, SHOW), Image.NEAREST)
    s = shade(s, [(x + 3, y - 1, rx, ry) for pose, x, y, rx, ry in sprites])
    for pose, x, y, rx, ry in sprites:
        sp = Image.open(os.path.join(ROOT, "assets", "sprites", "lili_src", pose + ".png")).convert("RGBA")
        s.paste(sp, (x - 48, y - 90), sp)
    if night:
        a = np.asarray(s).astype(float) * 0.45 + np.array([8, 12, 38])
        s = Image.fromarray(a.clip(0, 255).astype(np.uint8))
    m = Image.new("L", s.size, 0); ImageDraw.Draw(m).rounded_rectangle((0, 0, SHOW - 1, SHOW - 1), CORNER, fill=255)
    out = Image.new("RGB", s.size, (0, 0, 0)); out.paste(s, (0, 0), m)
    return out

def main():
    os.makedirs(os.path.join(OUT, "props"), exist_ok=True); os.makedirs(DOC, exist_ok=True)
    yards = {s: load_grid("yard_" + s) for s in SEASONS}
    for s, y in yards.items():
        y.save(os.path.join(OUT, f"yard_{s}.png"))
    room = load_grid("room")
    filled = room
    for box in PROPS.values():
        filled = fill_floor(filled, box)
    for pid, (sp, pos) in cut_props(room, filled).items():
        sp.save(os.path.join(OUT, "props", f"{pid}.png"))
        print(f"prop {pid}: {sp.size[0]}x{sp.size[1]} at {pos}")
    panes = pane_mask()
    rgba = np.dstack([np.asarray(filled), np.where(panes, 0, 255).astype(np.uint8)])
    Image.fromarray(rgba, "RGBA").save(os.path.join(OUT, "room.png"))
    x0, y0, x1, y1 = WINDOW
    props = cut_props(room, filled)
    lili_room = [("sit_S", 240, 285, 22, 6), ("sleep", 92, 262, 34, 8)]          # 480 px: pose, pivot, shadow
    for s in SEASONS:
        dv = door_view(yards[s]); dv.save(os.path.join(OUT, f"door_view_{s}.png"))
        for mode in ("day", "night") if s == "spring" else ("day",):
            comp = Image.new("RGB", (GRID, GRID)); comp.paste(dv, (x0, y0))
            comp.paste(Image.fromarray(rgba, "RGBA"), (0, 0), Image.fromarray(rgba, "RGBA"))
            if mode == "day":
                comp = sunbeam(comp)
            for pid in ("bowl_food", "bowl_water", "vase"):                      # contact shadows, light from top left
                sp, (px, py) = props[pid]
                comp = shade(comp, [prop_shadow(pid, sp, px, py)])
            for pid in ("pad", "vase", "bowl_food", "bowl_water"):
                sp, pos = props[pid]; comp.paste(sp, pos, sp)
            name = f"room_{s}_480.png" if mode == "day" else f"room_{s}_night_480.png"
            show(comp, lili_room, night=(mode == "night")).save(os.path.join(DOC, name))
            if mode == "day":
                comp.crop((150, 15, 250, 140)).resize((600, 750), Image.NEAREST).save(os.path.join(DOC, f"door_{s}_zoom.png"))
        show(yards[s], [("walk_SE", 240, 300, 26, 6)]).save(os.path.join(DOC, f"yard_{s}_480.png"))
    show(Image.fromarray(np.asarray(filled))).save(os.path.join(DOC, "room_empty_floor_480.png"))
    print("->", os.path.relpath(OUT, ROOT), "and", os.path.relpath(DOC, ROOT))

# --- the runtime light and shadow, simulated for the preview (the game does the same with remap tables) ---

def shade(img, ellipses, inner=0.62, outer=0.80):
    """a hard-edged two-step pixel-art shadow: the outer ring a little darker, the core darker, both a bit
    blue (the cold shadows of the art direction); ellipses = (cx, cy, rx, ry) in the image's pixels"""
    a = np.asarray(img).astype(float)
    yy, xx = np.mgrid[0:a.shape[0], 0:a.shape[1]]
    for cx, cy, rx, ry in ellipses:
        d = ((xx - cx) / rx) ** 2 + ((yy - cy) / ry) ** 2
        for lim, f in ((1.0, outer), (0.55, inner / outer)):
            m = d <= lim
            a[m] = a[m] * f + np.array([0, 4, 14]) * (1 - f)
    return Image.fromarray(a.clip(0, 255).astype(np.uint8))

def prop_shadow(pid, sp, px, py):
    """under the object's base, nudged right and down (the light comes from the top left)"""
    al = np.asarray(sp)[..., 3] > 0
    ys, xs = np.nonzero(al)
    base = ys.max()
    cols = xs[ys >= base - 3]
    cx = px + (cols.min() + cols.max()) / 2 + 2
    rx = (cols.max() - cols.min()) / 2 + 3
    return (cx, py + base - 1, rx, 4)

def sunbeam(img):
    """afternoon: the door's 3 x 3 panes land on the floor as a warm, widening patch (muntin gaps between)"""
    a = np.asarray(img).astype(float)
    top, bot = (150, 172, 228), (252, 150, 266)                 # y, x_left, x_right of the patch's edges
    for r in range(3):
        for c in range(3):
            ya = top[0] + (bot[0] - top[0]) * r / 3 + 2; yb = top[0] + (bot[0] - top[0]) * (r + 1) / 3 - 2
            for y in range(int(ya), int(yb)):
                t = (y - top[0]) / (bot[0] - top[0])
                xl = top[1] + (bot[1] - top[1]) * t; xr = top[2] + (bot[2] - top[2]) * t
                w = (xr - xl) / 3
                xa, xb = int(xl + w * c + 2), int(xl + w * (c + 1) - 2)
                a[y, xa:xb] = a[y, xa:xb] * 1.16 + np.array([30, 20, 2])
    return Image.fromarray(a.clip(0, 255).astype(np.uint8))

if __name__ == "__main__":
    main()
