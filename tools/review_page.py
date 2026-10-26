#!/usr/bin/env python3
"""Pixel Lili: one page to review the PixelLab animations (2026-10-23 night, reorganised 2026-10-26).

    py -3.12 tools/review_page.py        assets/pixellab/*_sheet.png + jobs.json + docs/img/anim/pixellab_<tag>.gif -> docs/review/

One row per animation, one column per direction (S, SW, W, NW, N, NE, E, SE), the variants (an earlier try, the
PixMiniMax remake) beside their direction. The older animations sit in their own rows too, marked when the game
already uses them (tools/pixellab_to_sprites.py ANIMS). A job without a sheet shows its start frame faded; a job
the owner rejected (jobs.json "redo") is marked until the remake replaces it. HIDDEN: sheets the game still uses
that the owner rejected, kept off the page. The GIFs come from tools/pixellab_import.py (one size, one ground line).
index.html links the images (light, committed); animaciok.html has them inside as data URIs (~10 MB, gitignored),
so it also shows in viewers that get the page alone, like the app's file preview. Nothing here goes into the game:
the owner approves first.
"""
import base64, html, json, os, re, sys
from collections import OrderedDict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from pixellab_norm import split, DIRS
from pixellab_to_sprites import ANIMS

OUT = os.path.join(ROOT, "docs", "review")
HIDDEN = {"drink_W": "background remnant at the rear (the owner, 2026-10-26); drink_W_pmm replaces it",
          "bark_S": "not the head throw Lili barked with (the owner, 2026-10-26); bark_S_v2 replaces it"}
VAR = {"": "", "pmm": "PixMiniMax", "v2": "2. változat", "v3": "v3", "try1": "1. próba"}

def main():
    jobs = json.load(open(os.path.join(ROOT, "assets", "pixellab", "jobs.json"), encoding="utf-8"))["jobs"]
    by_tag = {j["tag"]: j for j in jobs}
    in_game = {t for _, _, _, src in ANIMS.values() for t in src.values()}
    sheets = [f[:-10] for f in os.listdir(os.path.join(ROOT, "assets", "pixellab")) if f.endswith("_sheet.png")]
    tags = [t for t in sheets if t not in HIDDEN and os.path.exists(os.path.join(ROOT, "docs", "img", "anim", f"pixellab_{t}.gif"))]
    tags += [j["tag"] for j in jobs if j["tag"] not in sheets]          # not made yet
    groups = OrderedDict((split(j["tag"])[0], []) for j in jobs)
    for t in tags: groups.setdefault(split(t)[0], []).append(t)
    done = total = 0
    rows = []
    for name, ts in groups.items():
        if not ts: continue
        ts.sort(key=lambda t: (DIRS.index(split(t)[1]) if split(t)[1] in DIRS else 9, split(t)[2]))
        cells = []
        for t in ts:
            _, d, var = split(t)
            j = by_tag.get(t) or by_tag.get(f"{name}_{d}") or {}
            gif = os.path.join(ROOT, "docs", "img", "anim", f"pixellab_{t}.gif")
            total += 1
            if os.path.exists(gif) and t in sheets:
                img = f'<img src="../img/anim/pixellab_{t}.gif" alt="{t}">'
                if j.get("redo") and by_tag.get(t) is j: state, cls = "újra készül", "redo"
                else: state, cls = "kész", "done"; done += 1
            else:
                img = f'<img class="wait" src="../../assets/startframes/{j["start"]}.png" alt="{j["start"]}">'
                state, cls = "vár", "todo"
            tags_html = (f' <i>{VAR.get(var, var)}</i>' if var else "") + (' <em>a játékban</em>' if t in in_game else "")
            why = f'<br><span class="why">{html.escape(j["redo"])}</span>' if cls == "redo" else ""
            action = f'<br><span>{html.escape(j["action"])}</span>' if j.get("action") else ""
            cells.append(f'<figure class="{cls}">{img}<figcaption><b>{d or "-"}</b> {state}{tags_html}{action}{why}</figcaption></figure>')
        rows.append(f'<section><h2>{html.escape(name)}</h2><div class="row">{"".join(cells)}</div></section>')
    page = f"""<!doctype html><html lang="hu"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Lili animációk</title><style>
:root{{--bg:#1d1f24;--fg:#e8e6e1;--mute:#9a9790;--card:#2a2d33;--ok:#7fd1a8;--warn:#e8b45c;--game:#8fb8ff}}
body{{margin:0;padding:16px;background:var(--bg);color:var(--fg);font:14px/1.4 system-ui,sans-serif}}
h1{{font-size:20px;margin:0 0 4px}} h2{{font-size:15px;margin:18px 0 6px}} p{{color:var(--mute);margin:0 0 8px;max-width:70em}}
.row{{display:flex;flex-wrap:wrap;gap:8px}} figure{{margin:0;background:var(--card);border-radius:8px;padding:6px;width:200px}}
img{{width:192px;height:192px;image-rendering:pixelated;display:block;background:#34373e;border-radius:4px}}
img.wait{{opacity:.45}} figcaption{{font-size:12px;margin-top:4px}} figcaption span{{color:var(--mute)}}
.done b{{color:var(--ok)}} .redo b,.why{{color:var(--warn)!important}} .redo img{{opacity:.6}} i{{color:var(--mute)}} em{{color:var(--game);font-style:normal}}
</style></head><body><h1>Lili animációk: áttekintés</h1>
<p>{done} / {total} kész. Minden animáció egy méretben és egy talpvonalon (a 128 px-es cella alsó sora), ahogy a játékban egymás után jönnek.
Ami még nincs kész, ott a kezdőkocka látszik halványan; a narancs „újra készül” jelűeket elvetetted, az új változat váltja őket.
A <em>a játékban</em> jelűeket a játék már használja. Semmi új nem kerül be, amíg jóvá nem hagyod.</p>
{"".join(rows)}</body></html>"""
    os.makedirs(OUT, exist_ok=True)
    open(os.path.join(OUT, "index.html"), "w", encoding="utf-8").write(page)
    def inline(m):
        path = os.path.normpath(os.path.join(OUT, m.group(1)))
        kind = "gif" if path.endswith(".gif") else "png"
        return 'src="data:image/%s;base64,%s"' % (kind, base64.b64encode(open(path, "rb").read()).decode("ascii"))
    open(os.path.join(OUT, "animaciok.html"), "w", encoding="utf-8").write(re.sub(r'src="([^"]+\.(?:gif|png))"', inline, page))
    print(f"{done} / {total} done -> docs/review/index.html, docs/review/animaciok.html (self-contained)")

if __name__ == "__main__":
    main()
