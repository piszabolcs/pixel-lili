#!/usr/bin/env python3
"""Pixel Lili: summarise a labelled whistle recording (diag `label` run, `W,...` lines).

    py -3.12 tools/whistle_stats.py docs/logs/2026-10-19_whistle_labelled.log

Per label (1 silence, 2 whistle, 3 speech, 4 put down / pick up, 5 knock) the
percentiles of each feature, over the frames that matter: for the whistle label
only its tonal frames (the gaps between the five whistles are silence), for the
others every frame that the current detector calls a tone (those are the false
alarms the new gates must reject).
"""
import sys, re, statistics as st

NAMES = {1: "csend", 2: "fütty", 3: "beszéd", 4: "letevés", 5: "kopogás"}
FEAT = ["rms_l", "peak_db", "ratio_db", "frac", "low_rel_db"]

def pct(v, p):
    v = sorted(v); return v[min(len(v) - 1, int(p / 100 * len(v)))] if v else float("nan")

rows = []
for line in open(sys.argv[1], encoding="utf-8", errors="replace"):
    m = re.search(r"W,(\d+),(\d+),(.*)$", line.strip())
    if not m or not m.group(1).isdigit(): continue
    f = m.group(3).split(",")
    if len(f) < 8: continue
    rows.append(dict(label=int(m.group(2)), rms_l=float(f[0]), peak_hz=float(f[1]), peak_db=float(f[2]),
                     ratio_db=float(f[3]), frac=float(f[4]), low_rel_db=float(f[5]), tone=int(f[6])))
print(f"{len(rows)} frames")
for lab in sorted(NAMES):
    all_ = [r for r in rows if r["label"] == lab]
    sel = [r for r in all_ if r["tone"]]
    print(f"\n{NAMES[lab]}: {len(all_)} frames, {len(sel)} called tone by the current detector")
    if not sel: continue
    print(f"  {'feature':<11} {'p5':>7} {'p25':>7} {'p50':>7} {'p75':>7} {'p95':>7}")
    for k in FEAT:
        v = [r[k] for r in sel]
        print(f"  {k:<11} " + " ".join(f"{pct(v, p):>7.2f}" for p in (5, 25, 50, 75, 95)))
