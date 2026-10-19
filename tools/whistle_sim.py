#!/usr/bin/env python3
"""Pixel Lili: replay a labelled recording through candidate whistle detectors (PC side).

    py -3.12 tools/whistle_sim.py docs/logs/2026-10-19_whistle_labelled.log

The frames carry the features the panel computed (whistle.c), so a detector can
be tried here exactly as the firmware would run it, without reflashing. Prints
the events per label for the old detector and a small grid of new gates. The
wanted answer for the 2026-10-19 recording: 5 in "fütty", 0 everywhere else.
"""
import itertools, re, sys

NAMES = {1: "csend", 2: "fütty", 3: "beszéd", 4: "letevés", 5: "kopogás"}

def load(path):
    rows = []
    for line in open(path, encoding="utf-8", errors="replace"):
        m = re.search(r"W,(\d+),(\d+),(.*)$", line.strip())
        if not m: continue
        f = m.group(3).split(",")
        if len(f) < 8: continue
        rows.append(dict(label=int(m.group(2)), peak_hz=float(f[1]), peak_db=float(f[2]), ratio_db=float(f[3]),
                         frac=float(f[4]), low_rel_db=float(f[5])))
    return rows

def run(rows, ratio, frac, low, floor, frames=6, gap=1, tol=0.15, old=False, peak_min=-99.0, rearm=6):
    """events per label. old=True: the current firmware (gaps of 2 frames allowed,
    the reference frequency drifts with the peak, no frac / low gates)."""
    ev = {k: 0 for k in NAMES}; streak = 0; quiet = 0; ref = 0.0; armed = True; pmax = -99.0
    for r in rows:
        if old: tone = r["ratio_db"] > 12 and r["peak_db"] > -60
        else:   tone = r["ratio_db"] > ratio and r["frac"] >= frac and r["low_rel_db"] <= low and r["peak_db"] > floor
        if tone:
            if streak > 0 and abs(r["peak_hz"] - ref) <= tol * ref:
                streak += 1; pmax = max(pmax, r["peak_db"])
                if old: ref = 0.8 * ref + 0.2 * r["peak_hz"]
            else:
                streak = 1; ref = r["peak_hz"]; pmax = r["peak_db"]
            quiet = 0
            if old and streak == frames and r["label"] in ev: ev[r["label"]] += 1
            if not old and streak >= frames and armed and pmax >= peak_min and r["label"] in ev:
                ev[r["label"]] += 1; armed = False        # the firmware's v3 rule (whistle.c)
        else:
            quiet += 1
            if quiet > (2 if old else gap): streak = 0
            if quiet >= rearm: armed = True
    return ev

def fmt(ev): return "  ".join(f"{NAMES[k]} {ev[k]}" for k in NAMES)

def main():
    rows = load(sys.argv[1])
    print(f"{len(rows)} frames\nold detector:      {fmt(run(rows, 0, 0, 0, 0, frames=8, old=True))}")
    print(f"firmware v3:       {fmt(run(rows, 16, 0.80, 10, -65, frames=6, peak_min=-42))}\n")
    best = []
    for ratio, frac, low, floor in itertools.product((15, 18, 20, 22), (0.75, 0.8, 0.85, 0.9), (-5, 0, 5, 10), (-60, -55, -50)):
        ev = run(rows, ratio, frac, low, floor)
        false = sum(v for k, v in ev.items() if k != 2)
        best.append((false, abs(ev[2] - 5), -ratio, (ratio, frac, low, floor), ev))
    best.sort()
    print("best candidates (false alarms, then whistle count closest to 5, then the loosest ratio):")
    for false, miss, _, p, ev in best[:10]:
        print(f"  ratio>{p[0]:>2} frac>={p[1]:.2f} low<={p[2]:>3} floor>{p[3]}  ->  {fmt(ev)}")

if __name__ == "__main__":
    main()
