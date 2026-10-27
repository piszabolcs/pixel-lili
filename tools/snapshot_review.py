#!/usr/bin/env python3
"""Pixel Lili: the M3.7 approval page from the simulator's --snapshot scenes (spec 6.3, 12 M3.7 [G]).

    (WSL) cd sim && ./lilisim --snapshot ~/snap
    py -3.12 tools/snapshot_review.py <the snap dir, as a Windows path>

Every <view>_<season>_<daypart>.ppm becomes docs/img/snapshots/<name>.png (half size, 240 x 240: the panel's
logical canvas is 480, the page is for a phone), and docs/review/m3_7.html shows both views as a 4 x 4 table
(the seasons down, the dayparts across), every image inlined.
"""
import base64, io, os, sys
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VIEWS = (("room", "Szoba"), ("yard", "Udvar"))
SEASONS = (("spring", "tavasz"), ("summer", "nyár"), ("autumn", "ősz"), ("winter", "tél"))
DAYPARTS = (("morning", "reggel"), ("afternoon", "délután"), ("evening", "este"), ("night", "éjszaka"))
OPEN = ("Évszakos díszek (tavasz: virágos váza, nyár: dinnye, ősz: tök, tél: izzósor, hó, hóesés, hulló levelek, szirmok) "
        "és a különleges napok díszei (január, június 27., szeptember 12.) még nincsenek megrajzolva: ezekhez új grafika kell.")

def main():
    src = sys.argv[1]
    out_dir = os.path.join(ROOT, "docs", "img", "snapshots")
    os.makedirs(out_dir, exist_ok=True)
    parts = []
    for v, vname in VIEWS:
        rows = []
        for s, sname in SEASONS:
            cells = []
            for d, _ in DAYPARTS:
                name = f"{v}_{s}_{d}"
                im = Image.open(os.path.join(src, name + ".ppm")).convert("RGB")
                small = im.resize((im.width // 2, im.height // 2), Image.LANCZOS)
                small.save(os.path.join(out_dir, name + ".png"))
                buf = io.BytesIO(); small.save(buf, "PNG")
                cells.append(f'<td><img src="data:image/png;base64,{base64.b64encode(buf.getvalue()).decode()}" alt="{name}"></td>')
            rows.append(f"<tr><th>{sname}</th>{''.join(cells)}</tr>")
        head = "".join(f"<th>{dn}</th>" for _, dn in DAYPARTS)
        parts.append(f"<h2>{vname}</h2><div class=wrap><table><tr><th></th>{head}</tr>{''.join(rows)}</table></div>")
    html = ("<!doctype html><html lang=hu><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
            "<title>M3.7 jelenetek</title><style>body{font-family:system-ui,sans-serif;background:#f4ece4;color:#3b2418;margin:0;padding:16px}"
            "h1{font-size:1.3em}.wrap{overflow-x:auto}table{border-collapse:collapse}td,th{padding:3px;font-weight:600;font-size:.9em}"
            "img{width:160px;height:160px;display:block}</style>"
            "<h1>M3.7: a szoba és az udvar, 4 évszak x 4 napszak</h1>"
            "<p>A szimulátor <code>--snapshot</code> képei, Lili ülve középen. Soronként egy évszak, oszloponként egy napszak. "
            "Az udvaron a bokrok csak egyszer rezzennek meg, amikor Lili belemegy (M4.2); a madarak és a pillangó később jönnek.</p>"
            f"<p><b>{OPEN}</b></p>" + "".join(parts) + "</html>")
    out = os.path.join(ROOT, "docs", "review", "m3_7.html")
    open(out, "w", encoding="utf-8").write(html)
    print(f"-> {out} ({os.path.getsize(out) // 1024} KB), {out_dir}")

if __name__ == "__main__":
    main()
