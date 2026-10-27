#!/usr/bin/env python3
"""Pixel Lili: the approval material of an animation round (spec 7.5 B8, M3.5 [G]).

    py -3.12 tools/anim_review.py                 the M3.5 round (A01-A09 and stand_up)
    py -3.12 tools/anim_review.py m3_6            the M3.6 round (A10-A27 and the ones beyond the spec's list)
    py -3.12 tools/anim_review.py walk trot       only these (into the M3.5 page's files)

For each animation, from the game's room-size frames (assets/sprites/lili/<anim>/, tools/pixellab_to_sprites.py):
  docs/img/sheets/<anim>.png     the contact sheet: a row per direction, a column per frame, 2x, the pivot row in red
  docs/img/anim/m3x_<anim>.gif   the --anim-viewer as a GIF: the compass of 8 directions playing at once, at its fps
and docs/review/m3_x.html, one self-contained page (every image inlined) to look at on a phone. A direction an
animation lacks shows the nearest one it has, as the game does (marked with a * on the compass).
A reversed animation (stand_up = sit_down backwards, tools/pixellab_to_sprites.py REVERSED) is drawn from its
source's frames in the reverse order, as the game plays it.
"""
import base64, io, json, os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPR = os.path.join(ROOT, "assets", "sprites", "lili")
ROUNDS = {"m3_5": ("idle_stand", "idle_sit_blink", "sit_down", "stand_up", "walk", "trot", "run", "tiptoe", "scurry", "pant_smile"),
          "m3_6": ("head_tilt", "play_bow", "bark_head_flip", "chew_toy", "pet_enjoy", "eat_drink", "sleep_curl", "sun_sit", "squat",
                   "look_around", "sniff", "startle", "pounce", "ask_pet", "drink", "lie_down", "pee", "poop", "tug_rope", "eat_treat",
                   "carry_trot", "lie_down_rope", "chew_treat")}
HEAD = {"m3_5": "M3.5: az A01-A09 animációk mind a 8 irányból",
        "m3_6": "M3.6: az A10-A27 animációk és a lista feletti társaik"}
MISSING = {"m3_6": "Még nincs PixelLab-sheet (nem készült el, nincs bekötve): A13 puffogás (huff), A14 felvétel / leejtés (pickup / drop), "
                   "A21 bűnös kukucskálás a sarokülő alól (guilty_peek), A22 fel- és leugrás az ágyra (jump_up / jump_down)."}
TITLE = {"idle_stand": "A01 álló idle", "idle_sit_blink": "A02 ülő idle, pislogás", "sit_down": "A03 leülés",
         "stand_up": "A03 felállás (a leülés visszafelé)", "walk": "A04 járás", "trot": "A05 ügetés", "run": "A06 futás",
         "tiptoe": "A07 lopakodás", "scurry": "A08 surranás", "pant_smile": "A09 lihegő mosoly",
         "head_tilt": "A10 fejbillentés", "play_bow": "A11 meghajlás (játékra hívás)", "bark_head_flip": "A12 ugatás fejfelcsapással",
         "chew_toy": "A15 kötélrágás fekve", "pet_enjoy": "A16 simogatás élvezése", "eat_drink": "A17 evés a tálból",
         "sleep_curl": "A18 alvás összegömbölyödve", "sun_sit": "A19 napozás ülve", "squat": "A20 leguggolás",
         "look_around": "A23 körülnézés", "sniff": "A24 szaglászás", "startle": "A25 megriadás", "pounce": "A26 ráugrás",
         "ask_pet": "A27 simiért kérés", "drink": "A17b ivás a tálból", "lie_down": "lefekvés", "pee": "pisilés",
         "poop": "kakilás", "tug_rope": "kötélhúzás", "eat_treat": "keksz evése", "carry_trot": "ügetés kötéllel a szájában",
         "lie_down_rope": "lefekvés kötéllel", "chew_treat": "keksz rágása fekve"}
REVERSED = {"stand_up": "sit_down"}
DIRS = ("S", "SW", "W", "NW", "N", "NE", "E", "SE")
GRID = {0: (1, 2), 1: (0, 2), 2: (0, 1), 3: (0, 0), 4: (1, 0), 5: (2, 0), 6: (2, 1), 7: (2, 2)}   # the viewer's compass
FLOOR, LINE, INK, RED = (190, 140, 112), (160, 112, 88), (59, 36, 24), (220, 40, 40)
SCALE = 2
try: FONT = ImageFont.truetype("arial.ttf", 13)              # the default bitmap font has no accents
except OSError: FONT = ImageFont.load_default()

def load(anim):
    src = REVERSED.get(anim, anim)
    man = json.load(open(os.path.join(SPR, src, "manifest.json"), encoding="utf-8"))
    frames = {}
    for d in man["dirs"]:
        seq = []
        for i in range(man["frames"]):
            im = Image.open(os.path.join(SPR, src, f"d{d}_f{i}.png"))
            rgba = im.convert("RGBA")
            if im.mode == "P":                                   # index 0 is transparent in every sprite
                a = np.array(rgba); a[np.array(im) == 0, 3] = 0; rgba = Image.fromarray(a)
            seq.append(rgba)
        frames[d] = seq[::-1] if anim in REVERSED else seq
    return man, frames

def sheet(anim, man, frames):
    cw, ch = man["cell_w"] * SCALE, man["cell_h"] * SCALE
    lab, pad = 34, 6
    n = man["frames"]
    img = Image.new("RGB", (lab + n * (cw + pad) + pad, len(frames) * (ch + pad) + pad + 18), FLOOR)
    dr = ImageDraw.Draw(img)
    dr.text((pad, 3), f"{TITLE.get(anim, anim)}: {n} kocka, {man['fps']} fps, {'ismétlődik' if man['loop'] else 'egyszer'}", fill=INK, font=FONT)
    py = man["pivot"][1] * SCALE
    for r, d in enumerate(sorted(frames)):
        y = 18 + pad + r * (ch + pad)
        dr.text((pad, y + ch // 2 - 6), DIRS[d], fill=INK, font=FONT)
        for i, f in enumerate(frames[d]):
            x = lab + pad + i * (cw + pad)
            dr.rectangle((x, y, x + cw - 1, y + ch - 1), outline=LINE)
            img.paste(f.resize((cw, ch), Image.NEAREST), (x, y), f.resize((cw, ch), Image.NEAREST))
            dr.line((x, y + py - 1, x + cw - 1, y + py - 1), fill=RED)     # over the paws: they stand on it
    return img

def viewer_gif(man, frames):
    cell = 120
    w = 3 * cell
    out = []
    for i in range(man["frames"]):
        img = Image.new("RGB", (w, w), FLOOR)
        dr = ImageDraw.Draw(img)
        for k in (1, 2):
            dr.line((k * cell, 0, k * cell, w), fill=LINE); dr.line((0, k * cell, w, k * cell), fill=LINE)
        for d in range(8):
            gx, gy = GRID[d]
            f = frames.get(d) or frames[min(frames, key=lambda e: min((e - d) % 8, (d - e) % 8))]   # the nearest it has, as the game
            px, py = gx * cell + cell // 2, gy * cell + cell * 4 // 5
            dr.line((px - 30, py, px + 30, py), fill=LINE)
            img.paste(f[i], (px - man["pivot"][0], py - man["pivot"][1]), f[i])
            dr.text((gx * cell + 4, gy * cell + 3), DIRS[d] + ("" if d in frames else " *"), fill=INK, font=FONT)
        n = man["frames"]
        for j in range(n):
            x0 = cell + (cell - n * 10) // 2 + j * 10
            dr.rectangle((x0, cell + cell // 2 - 4, x0 + 7, cell + cell // 2 + 3), fill=(255, 255, 255) if j == i else LINE)
        out.append(img.resize((w * SCALE, w * SCALE), Image.NEAREST))
    return out

def b64(img, fmt="PNG", **kw):
    buf = io.BytesIO(); img.save(buf, fmt, **kw); return base64.b64encode(buf.getvalue()).decode()

def main():
    rnd = sys.argv[1] if len(sys.argv) == 2 and sys.argv[1] in ROUNDS else "m3_5"
    anims = ROUNDS[rnd] if len(sys.argv) == 1 or rnd != "m3_5" or sys.argv[1:] == ["m3_5"] else sys.argv[1:]
    os.makedirs(os.path.join(ROOT, "docs", "img", "sheets"), exist_ok=True)
    rows = []
    for anim in anims:
        man, frames = load(anim)
        sh = sheet(anim, man, frames)
        sh.save(os.path.join(ROOT, "docs", "img", "sheets", f"{anim}.png"))
        gif = viewer_gif(man, frames)
        dur = int(1000 / man["fps"])
        loop = 0 if man["loop"] else 0                      # the page loops the one-shots too, so they can be watched
        gif_path = os.path.join(ROOT, "docs", "img", "anim", f"{rnd.replace('_', '')}_{anim}.gif")
        gif[0].save(gif_path, save_all=True, append_images=gif[1:], duration=[dur] * (len(gif) - 1) + [dur if man["loop"] else 900], loop=loop)
        gif_b64 = base64.b64encode(open(gif_path, "rb").read()).decode()
        rows.append(f'<section><h2>{TITLE.get(anim, anim)}</h2><p>{man["frames"]} kocka, {man["fps"]} fps, '
                    f'{"ismétlődik" if man["loop"] else "egyszer játszódik le (itt a nézegetéshez ismétlődik)"}; '
                    f'cella {man["cell_w"]}x{man["cell_h"]} px (szobai méret)</p>'
                    f'<img class="gif" src="data:image/gif;base64,{gif_b64}" alt="{anim}">'
                    f'<details><summary>Kontaktlap (irányonként a kockák)</summary><img class="sheet" src="data:image/png;base64,{b64(sh)}" alt="{anim} kontaktlap"></details></section>')
        print(f"{anim}: sheet + gif")
    html = ("<!doctype html><html lang=hu><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
            "<title>M3.5 animációk</title><style>body{font-family:system-ui,sans-serif;background:#f4ece4;color:#3b2418;margin:0;padding:16px;max-width:760px}"
            "h1{font-size:1.3em}h2{font-size:1.05em;margin:1.4em 0 .3em}p{margin:.2em 0 .6em}img{image-rendering:pixelated;max-width:100%;height:auto;display:block}"
            "details{margin:.5em 0}summary{cursor:pointer}</style>"
            f"<h1>{HEAD[rnd]}</h1>"
            "<p>Így néz ki Lili a játékban, szobai méretben (0,75). Minden animációnál az iránytű: fent hátulról (N), lent szemből (S), "
            "középen a kockaszámláló. A kontaktlapon soronként egy irány, a piros vonal a talpvonal. A felállás a leülés kockái visszafelé. A csillagos irányban az animációnak nincs saját képe, ott a legközelebbi irányét mutatja, ahogy a játék is. "
            "A szimulátorban ugyanez: <code>./lilisim --anim-viewer</code> (A / nyilak: következő, Tab: udvari méret, Space: megállít).</p>"
            + (f"<p><b>{MISSING[rnd]}</b></p>" if rnd in MISSING else "") + "".join(rows) + "</html>")
    out = os.path.join(ROOT, "docs", "review", f"{rnd}.html")
    open(out, "w", encoding="utf-8").write(html)
    print(f"-> {out} ({os.path.getsize(out) // 1024} KB)")

if __name__ == "__main__":
    main()
