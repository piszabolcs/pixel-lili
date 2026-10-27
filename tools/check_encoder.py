#!/usr/bin/env python3
"""Pixel Lili: the C encoder and the Python one agree (spec 12 M6.1).

    (WSL) cd sim && ./lilisim --dump-prompts /tmp/p.txt 1000
    py -3.12 tools/check_encoder.py <that file>

Each line holds a situation's 24 field tokens and the ids the C encoder made of them; model/schema_lili.py
encodes the same tokens again. Exit 0 = all identical.
"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "model"))
import schema_lili as S

def main():
    bad = n = 0
    for line in open(sys.argv[1], encoding="utf-8"):
        toks, ids = line.split("|")
        toks = toks.split(); ids = [int(x) for x in ids.split()]
        state = {name: tok for (name, _, _), tok in zip(S.FIELDS, toks)}
        py = S.encode(state)
        n += 1
        if py != ids:
            bad += 1
            if bad <= 5: print("differs:", toks, ids, py)
    print(f"check_encoder: {n} situations, {n - bad} identical, {bad} different")
    sys.exit(1 if bad or not n else 0)

if __name__ == "__main__":
    main()
