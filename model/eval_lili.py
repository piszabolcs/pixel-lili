#!/usr/bin/env python3
"""Pixel Lili: how well a brain model learned the teacher (spec 10.11, M7.4).

    python3 model/eval_lili.py --size S        model/out/lili/ckpt_S.pt on val.npz and probes.jsonl -> eval_S.json

Constrained decoding (spec 10.5): each answer slot may only be a token of its class (A1-A3 actions, then a gait,
a face, a bubble, an urgency), so 100 % of the answers are valid by construction; the first action is further
masked to the actions possible there (the probes carry the mask). Measures:
  A1 = the teacher's likeliest first action (fresh validation situations, seeds the training never saw; the
       model's choice masked to the possible actions, and either of the teacher's two when they tie) >= 80 %
  the teacher's own sampled first action in the model's top-2 >= 93 %
  the probes P1-P12 (P6 is a scene of the time away, measured in the simulator): the model's probability of the
  probe's actions, averaged over its samples, at the device's temperature 0.8
"""
import argparse, json, os, sys
import numpy as np
import torch

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import lili_model, schema_lili as S

P_LEN = 26
SLOTS = S.SLOTS                                               # (name, first id, n)

def load(size, dev):
    ck = torch.load(os.path.join(HERE, "out", "lili", f"ckpt_{size}.pt"), map_location=dev, weights_only=False)
    m = lili_model.Transformer(lili_model.ModelArgs(**{**ck["args"], "dropout": 0.0})).to(dev)
    m.load_state_dict(ck["model"]); m.eval()
    return m, ck

def a1_logits(model, prompts):
    with torch.no_grad():
        lg = model(prompts)[:, P_LEN - 1].float()             # the position after "=>" predicts A1
    first, n = SLOTS[0][1], SLOTS[0][2]
    return lg[:, first:first + n]

def decode(model, prompt, dev):
    """the 7 answer tokens, greedy, each slot constrained to its class"""
    seq = list(prompt)
    for _, first, n in SLOTS:
        with torch.no_grad():
            lg = model(torch.tensor([seq], device=dev))[0, -1, first:first + n]
        seq.append(first + int(lg.argmax()))
    return seq[P_LEN:]

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--size", default="S"); a = ap.parse_args()
    dev = "cuda" if torch.cuda.is_available() else "cpu"
    model, ck = load(a.size, dev)
    va = np.load(os.path.join(HERE, "out", "lili", "val.npz"))
    seq, best, bestp, amask = va["seq"].astype(np.int64), va["best"].astype(np.int64), va["bestp"], va["mask"]
    first, n_act = SLOTS[0][1], SLOTS[0][2]
    bits = ((amask[:, None] >> np.arange(n_act, dtype=np.uint64)[None, :]) & np.uint64(1)).astype(bool)   # the environment mask
    tie = np.abs(bestp[:, 0] - bestp[:, 1]) < 1e-3                       # the teacher's two likeliest are equally likely
    agree = top2 = 0
    for i in range(0, len(seq), 2048):
        x = torch.from_numpy(seq[i:i + 2048, :P_LEN]).to(dev)
        lg = a1_logits(model, x)
        lg = lg.masked_fill(~torch.from_numpy(bits[i:i + 2048]).to(dev), float("-inf"))
        order = lg.argsort(dim=1, descending=True)[:, :2].cpu().numpy() + first
        b = best[i:i + 2048]
        agree += int(((order[:, 0] == b[:, 0]) | (tie[i:i + 2048] & (order[:, 0] == b[:, 1]))).sum())
        top2 += int(((order[:, 0] == seq[i:i + 2048, P_LEN]) | (order[:, 1] == seq[i:i + 2048, P_LEN])).sum())
    n = len(seq)
    valid = all(SLOTS[k][1] <= t < SLOTS[k][1] + SLOTS[k][2] for k, t in enumerate(decode(model, list(seq[0, :P_LEN]), dev)))
    res = {"size": a.size, "params": sum(p.numel() for p in model.parameters()), "val_loss": ck["val"],
           "a1_agree": round(agree / n, 4), "label_in_top2": round(top2 / n, 4), "valid_answer": valid, "probes": {}}
    print(f"eval {a.size}: A1 = the teacher's likeliest {res['a1_agree']:.1%} (>= 80 %), the teacher's label in the top-2 {res['label_in_top2']:.1%} (>= 93 %)")
    # the probes, with the environment mask on A1 and the device's temperature
    by = {}
    for line in open(os.path.join(HERE, "out", "lili", "probes.jsonl"), encoding="utf-8"):
        o = json.loads(line); by.setdefault(o["probe"], []).append(o)
    ok_all = True
    for name, rows in by.items():
        x = torch.tensor([r["p"] for r in rows], device=dev)
        lg = a1_logits(model, x) / 0.8
        mask = torch.full_like(lg, float("-inf"))
        for i, r in enumerate(rows):
            for t in r["mask"]: mask[i, t - first] = 0
        p = torch.softmax(lg + mask, dim=1).cpu().numpy()
        val = float(np.mean([sum(p[i, t - first] for t in r["set"]) for i, r in enumerate(rows)]))
        thr, at_most = rows[0]["thr"], rows[0]["at_most"]
        ok = val <= thr + 1e-6 if at_most else val >= thr
        ok_all &= ok
        res["probes"][name] = {"p": round(val, 3), "thr": thr, "at_most": bool(at_most), "ok": ok}
        print(f"  {name:4s} {val:.3f} {'<=' if at_most else '>='} {thr:.2f}  {'ok' if ok else 'MISS'}")
    res["probes_ok"] = ok_all
    json.dump(res, open(os.path.join(HERE, "out", "lili", f"eval_{a.size}.json"), "w"), indent=1)
    print(f"eval {a.size}: probes {'all ok' if ok_all else 'some missed'}")

if __name__ == "__main__":
    main()
