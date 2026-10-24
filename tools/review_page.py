#!/usr/bin/env python3
"""Pixel Lili: one page to review the PixelLab batch (2026-10-23 night).

    py -3.12 tools/review_page.py        assets/pixellab/jobs.json + docs/img/anim/pixellab_<tag>.gif -> docs/review/index.html

Every job in jobs.json, grouped by animation, one column per direction: the GIF if it is made (tools/pixellab_import.py),
the start frame otherwise, the prompt under it. Nothing here goes into the game: the owner approves first.
"""
import html, json, os, re
from collections import OrderedDict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "docs", "review")
D8 = ["S", "SW", "W", "NW", "N", "NE", "E", "SE"]

def main():
    jobs = json.load(open(os.path.join(ROOT, "assets", "pixellab", "jobs.json"), encoding="utf-8"))["jobs"]
    groups = OrderedDict()
    for j in jobs:
        m = re.match(r"(.+)_(S|SW|W|NW|N|NE|E|SE)$", j["tag"])
        name, d = (m.group(1), m.group(2)) if m else (j["tag"], "")
        groups.setdefault(name, {})[d] = j
    done = total = 0
    rows = []
    for name, by_dir in groups.items():
        cells = []
        for d in D8 + [""]:
            j = by_dir.get(d)
            if not j: continue
            total += 1
            gif = os.path.join(ROOT, "docs", "img", "anim", f"pixellab_{j['tag']}.gif")
            if os.path.exists(gif):
                done += 1
                img = f'<img src="../img/anim/pixellab_{j["tag"]}.gif" alt="{j["tag"]}">'
                state = "kész"
            else:
                img = f'<img class="wait" src="../../assets/startframes/{j["start"]}.png" alt="{j["start"]}">'
                state = "vár"
            cells.append(f'<figure class="{ "done" if state == "kész" else "todo" }">{img}<figcaption><b>{d or "-"}</b> {state}'
                         f'<br><span>{html.escape(j["action"])}</span></figcaption></figure>')
        rows.append(f'<section><h2>{html.escape(name)}</h2><div class="row">{"".join(cells)}</div></section>')
    # sheets outside jobs.json: first tries (*_try1), alternatives (*_pmm, *_v2) and the older single animations
    tags = {j["tag"] for j in jobs}
    extra = sorted(f[:-10] for f in os.listdir(os.path.join(ROOT, "assets", "pixellab"))
                   if f.endswith("_sheet.png") and f[:-10] not in tags
                   and os.path.exists(os.path.join(ROOT, "docs", "img", "anim", f"pixellab_{f[:-10]}.gif")))
    if extra:
        cells = [f'<figure class="done"><img src="../img/anim/pixellab_{t}.gif" alt="{t}"><figcaption><b>{html.escape(t)}</b></figcaption></figure>'
                 for t in extra]
        rows.append(f'<section><h2>Egyéb változatok (próbák, alternatívák, a régebbi animációk)</h2><div class="row">{"".join(cells)}</div></section>')
    page = f"""<!doctype html><html lang="hu"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Lili animációk</title><style>
:root{{--bg:#1d1f24;--fg:#e8e6e1;--mute:#9a9790;--card:#2a2d33;--ok:#7fd1a8}}
body{{margin:0;padding:16px;background:var(--bg);color:var(--fg);font:14px/1.4 system-ui,sans-serif}}
h1{{font-size:20px;margin:0 0 4px}} h2{{font-size:15px;margin:18px 0 6px}} p{{color:var(--mute);margin:0 0 8px}}
.row{{display:flex;flex-wrap:wrap;gap:8px}} figure{{margin:0;background:var(--card);border-radius:8px;padding:6px;width:200px}}
img{{width:192px;height:192px;image-rendering:pixelated;display:block;background:#34373e;border-radius:4px}}
img.wait{{opacity:.45}} figcaption{{font-size:12px;margin-top:4px}} figcaption span{{color:var(--mute)}} .done b{{color:var(--ok)}}
</style></head><body><h1>Lili animációk: áttekintés</h1><p>{done} / {total} kész. Ami még nincs kész, ott a kezdőkocka látszik halványan. Semmi nincs beépítve, előbb te hagyod jóvá.</p>
{"".join(rows)}</body></html>"""
    os.makedirs(OUT, exist_ok=True)
    open(os.path.join(OUT, "index.html"), "w", encoding="utf-8").write(page)
    print(f"{done} / {total} done -> docs/review/index.html")

if __name__ == "__main__":
    main()
