#!/usr/bin/env python3
"""Pixel Lili: train the brain model on the teacher's pairs (spec 10.9, M7.3).

    python3 model/train_lili.py --size S            model/out/lili/{train,val}.npz -> model/out/lili/ckpt_S.pt, curve_S.json

A pair is the 26 prompt tokens and the 7 answer tokens (33); the loss is counted on the answer only (the prompt's
positions are masked), so the model spends itself on the decision, not on guessing the situation. AdamW, cosine
learning rate with warm-up, bf16 autocast on the GPU, fp32 master weights; the validation loss every 250 steps and
the best checkpoint kept.
"""
import argparse, json, math, os, sys, time
import numpy as np
import torch
import torch.nn.functional as F

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import lili_model

P_LEN, T_LEN = 26, 7

def batches(seq, batch, rng):
    while True:
        idx = rng.integers(0, len(seq), batch)
        yield torch.from_numpy(seq[idx].astype(np.int64))

def loss_of(model, x):
    logits = model(x[:, :-1])                                   # predicts tokens 1..32
    tgt = x[:, 1:]
    out = logits[:, P_LEN - 1:P_LEN - 1 + T_LEN]               # the positions that predict the 7 answer tokens
    return F.cross_entropy(out.reshape(-1, out.shape[-1]).float(), tgt[:, P_LEN - 1:P_LEN - 1 + T_LEN].reshape(-1))

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--size", default="S", choices=list(lili_model.SIZES))
    ap.add_argument("--iters", type=int, default=6000)
    ap.add_argument("--batch", type=int, default=128)
    ap.add_argument("--lr", type=float, default=6e-4)
    ap.add_argument("--warmup", type=int, default=200)
    ap.add_argument("--wd", type=float, default=0.1)
    ap.add_argument("--dropout", type=float, default=0.1)
    ap.add_argument("--seed", type=int, default=1337)
    a = ap.parse_args()
    torch.manual_seed(a.seed); rng = np.random.default_rng(a.seed)
    dev = "cuda" if torch.cuda.is_available() else "cpu"
    data = os.path.join(HERE, "out", "lili")
    tr = np.load(os.path.join(data, "train.npz"))["seq"]; va = np.load(os.path.join(data, "val.npz"))["seq"]
    model = lili_model.make(a.size, a.dropout).to(dev)
    n_par = sum(p.numel() for p in model.parameters())
    decay = [p for n, p in model.named_parameters() if p.dim() >= 2]; nodecay = [p for n, p in model.named_parameters() if p.dim() < 2]
    opt = torch.optim.AdamW([{"params": decay, "weight_decay": a.wd}, {"params": nodecay, "weight_decay": 0.0}], lr=a.lr, betas=(0.9, 0.95))
    lr_at = lambda it: a.lr * it / a.warmup if it < a.warmup else 0.1 * a.lr + 0.9 * a.lr * 0.5 * (1 + math.cos(math.pi * (it - a.warmup) / max(1, a.iters - a.warmup)))
    gen = batches(tr, a.batch, rng)
    vx = torch.from_numpy(va[:4096].astype(np.int64)).to(dev)
    curve, best, t0 = [], 1e9, time.time()
    ctx = torch.autocast("cuda", dtype=torch.bfloat16) if dev == "cuda" else torch.autocast("cpu", enabled=False)
    print(f"train {a.size}: {n_par / 1e6:.2f}M parameters, {len(tr)} train / {len(va)} val pairs, {dev}")
    for it in range(a.iters + 1):
        if it % 250 == 0 or it == a.iters:
            model.eval()
            with torch.no_grad(), ctx: vl = sum(loss_of(model, vx[i:i + 1024]).item() for i in range(0, len(vx), 1024)) / math.ceil(len(vx) / 1024)
            model.train()
            curve.append({"it": it, "val": round(vl, 4), "s": round(time.time() - t0, 1)})
            print(f"  it {it:5d}  val {vl:.4f}  {time.time() - t0:6.1f} s", flush=True)
            if vl < best:
                best = vl
                torch.save({"size": a.size, "args": vars(model.params), "model": model.state_dict(), "it": it, "val": vl}, os.path.join(data, f"ckpt_{a.size}.pt"))
        if it == a.iters: break
        for g in opt.param_groups: g["lr"] = lr_at(it)
        x = next(gen).to(dev, non_blocking=True)
        with ctx: loss = loss_of(model, x)
        opt.zero_grad(set_to_none=True); loss.backward(); torch.nn.utils.clip_grad_norm_(model.parameters(), 1.0); opt.step()
    json.dump({"size": a.size, "params": n_par, "best_val": best, "seconds": round(time.time() - t0, 1), "curve": curve},
              open(os.path.join(data, f"curve_{a.size}.json"), "w"), indent=1)
    print(f"train {a.size}: best val {best:.4f} in {time.time() - t0:.0f} s -> ckpt_{a.size}.pt")

if __name__ == "__main__":
    main()
