#!/usr/bin/env python3
"""Pixel Lili: the training pairs, cleaned, split and counted (spec 10.8, M7.2).

    python3 model/prep_lili.py              model/out/lili/traces_s*.jsonl -> model/out/lili/{train,val}.npz, stats.json

Duplicates: a (prompt, answer) pair seen more than 8 times keeps 8 (the answer's spread for a prompt stays, a
thousand identical nights do not drown the rest). The split is by seed (seeds 1-3 validation, the rest training:
no situation leaks from one to the other). The counts: per habit (the template's H), per first action, per input
field value. A habit under 1.2 % of the training rows is repeated up to 1.2 % (M7.2: none under 1 %).
"""
import collections, glob, json, os, re, sys
import numpy as np
import yaml

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import schema_lili as S

VAL_SEEDS = {1, 2, 3}
CAP = 8

def main():
    rules = yaml.safe_load(open(os.path.join(HERE, "teacher_rules.yaml"), encoding="utf-8"))
    pairs = collections.Counter(); seeds = {}; meta = {}
    n_raw = 0
    for path in sorted(glob.glob(os.path.join(HERE, "out", "lili", "traces_s*.jsonl"))):
        for line in open(path, encoding="utf-8"):
            o = json.loads(line); n_raw += 1
            key = (tuple(o["p"]), tuple(o["t"]))
            pairs[key] += 1; seeds.setdefault(key, o["s"]); meta.setdefault(key, (o["r"], tuple(o["b"]), tuple(o["bp"]), int(o["m"], 16)))
    rows = {"train": [], "val": []}
    for key, n in pairs.items():
        split = "val" if seeds[key] in VAL_SEEDS else "train"
        for _ in range(min(n, CAP)): rows[split].append((key, meta[key]))
    # a rare habit (under 1.2 % of the training rows) is repeated up to 1.2 %: spec 12 M7.2, no habit under 1 %
    by_habit = collections.defaultdict(list)
    for row in rows["train"]: by_habit[rules[row[1][0]]["habit"]].append(row)
    target = int(0.012 * len(rows["train"])) + 1
    boosted = {}
    for h, rs in by_habit.items():
        if 0 < len(rs) < target:
            extra = [rs[i % len(rs)] for i in range(target - len(rs))]
            rows["train"] += extra; boosted[h] = len(extra)
    out = os.path.join(HERE, "out", "lili")
    stats = {"raw": n_raw, "unique": len(pairs), "boosted": boosted}
    for split, rs in rows.items():
        seq = np.array([list(k[0]) + list(k[1]) for k, _ in rs], dtype=np.uint8)
        rule = np.array([m[0] for _, m in rs], dtype=np.int16)
        best = np.array([list(m[1]) for _, m in rs], dtype=np.uint8)
        bestp = np.array([list(m[2]) for _, m in rs], dtype=np.float32)
        mask = np.array([m[3] for _, m in rs], dtype=np.uint64)            # the possible first actions (bit a = action a)
        np.savez_compressed(os.path.join(out, f"{split}.npz"), seq=seq, rule=rule, best=best, bestp=bestp, mask=mask)
        stats[split] = len(rs)
    habit = collections.Counter(); act = collections.Counter(); tmpl = collections.Counter()
    for (k, (r, *_)) in rows["train"]:
        habit[rules[r]["habit"]] += 1; tmpl[rules[r]["id"]] += 1; act[S.TOKENS[k[1][0]]] += 1
    n = max(1, stats["train"])
    stats["habits"] = {h: round(100.0 * habit[h] / n, 2) for h in sorted(habit, key=lambda h: int(h[1:]))}
    stats["templates"] = {t: round(100.0 * c / n, 2) for t, c in tmpl.most_common()}
    stats["first_actions"] = {a: round(100.0 * c / n, 2) for a, c in act.most_common()}
    stats["low_habits"] = [h for h in (f"H{i}" for i in range(1, 16)) if stats["habits"].get(h, 0) < 1.0]
    json.dump(stats, open(os.path.join(out, "stats.json"), "w", encoding="utf-8"), indent=1, ensure_ascii=False)
    print(f"prep: {n_raw} pairs, {len(pairs)} different -> train {stats['train']}, val {stats['val']} (cap {CAP} per pair, val seeds {sorted(VAL_SEEDS)})")
    print("habits %:", ", ".join(f"{h} {v}" for h, v in stats["habits"].items()))
    print("repeated up to 1.2 %:", boosted or "none", "| under 1 % now:", stats["low_habits"] or "none")

if __name__ == "__main__":
    main()
