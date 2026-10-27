#!/usr/bin/env python3
"""Pixel Lili: the fp32 model's first actions for the device engine to match (spec 10.11, M8.1).

    python3 model/q4check_lili.py --size S        -> model/out/lili/q4check_S.jsonl (2000 validation prompts)

Each line: a prompt, the fp32 model's two likeliest first actions (constrained to the action class, no environment
mask) and the gap between their probabilities; lilisim --selftest-llm runs the 4-bit file on the same prompts. A
quarter of the situations are near-ties of the teacher's (pickup_rope | pickup_plush ...), where the argmax is a
coin toss for either model: the agreement is counted where fp32 is decided (gap > 0.05, >= 95 %), and the 4-bit
choice must always be one of fp32's two (docs/decisions.md, M8.1).
"""
import argparse, json, os, sys
import numpy as np
import torch

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import eval_lili, schema_lili as S

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--size", default="S"); ap.add_argument("--n", type=int, default=2000); a = ap.parse_args()
    model, _ = eval_lili.load(a.size, "cpu")
    seq = np.load(os.path.join(HERE, "out", "lili", "val.npz"))["seq"][: a.n].astype(np.int64)
    x = torch.from_numpy(seq[:, :eval_lili.P_LEN])
    with torch.no_grad():
        pr = torch.softmax(eval_lili.a1_logits(model, x).float(), dim=1)
    top = pr.topk(2, dim=1)
    a1 = top.indices[:, 0].numpy() + S.SLOTS[0][1]; a2 = top.indices[:, 1].numpy() + S.SLOTS[0][1]
    margin = (top.values[:, 0] - top.values[:, 1]).numpy()
    out = os.path.join(HERE, "out", "lili", f"q4check_{a.size}.jsonl")
    with open(out, "w") as f:
        for p, t, t2, m in zip(seq[:, :eval_lili.P_LEN].tolist(), a1.tolist(), a2.tolist(), margin.tolist()):
            f.write(json.dumps({"p": p, "a1": t, "a2": t2, "m": round(float(m), 4)}) + "\n")
    print(f"q4check {a.size}: {len(seq)} prompts -> {out}")

if __name__ == "__main__":
    main()
