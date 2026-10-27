#!/usr/bin/env python3
"""Pixel Lili: a trained brain model as the device engine's 4-bit file (spec 10.10, M8.1).

    python3 model/export_lili_q4.py --size S        model/out/lili/ckpt_S.pt -> model/out/lili/lili_S_q4.bin

The pocket-tank's "version 3" layout (model/export_q4.py, read by common/llm/q4_model.c unchanged): a 256-byte
header (magic "ak42", version 3, dim, hidden, layers, heads, kv heads, vocab, seq len, shared classifier, group
size), the fp32 norms, then every weight matrix as packed signed 4-bit nibbles with fp16 scales per group of 64.
After it a trailer the engine does not read but the firmware checks: "LSCH", the schema version, a hash of the
vocabulary (FNV-1a over the token names) and the token count; a model of another schema is refused and the rule
brain keeps running (spec 10.10).
"""
import argparse, os, struct, sys
import numpy as np
import torch

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import lili_model, schema_lili as S

GS = 64

def vocab_hash():
    h = 2166136261
    for t in S.TOKENS:
        for b in (t + "\n").encode("utf-8"): h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return h

def q4(w):
    w = w.detach().float().reshape(-1, GS)
    m = w.abs().max(dim=1).values
    sc = torch.where(m > 0, m / 7.0, torch.ones_like(m))
    q = torch.clamp(torch.round(w / sc[:, None]), -8, 7).to(torch.int16).view(-1)
    u = (q + 8).to(torch.uint8).numpy()
    return (u[0::2] | (u[1::2] << 4)).astype(np.uint8), sc.to(torch.float16).numpy()

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--size", default="S"); ap.add_argument("--out", default=None); a = ap.parse_args()
    ck = torch.load(os.path.join(HERE, "out", "lili", f"ckpt_{a.size}.pt"), map_location="cpu", weights_only=False)
    m = lili_model.Transformer(lili_model.ModelArgs(**{**ck["args"], "dropout": 0.0})); m.load_state_dict(ck["model"])
    p = m.params
    ws = [m.tok_embeddings.weight] + [l.attention.wq.weight for l in m.layers] + [l.attention.wk.weight for l in m.layers] + \
         [l.attention.wv.weight for l in m.layers] + [l.attention.wo.weight for l in m.layers] + [l.feed_forward.w1.weight for l in m.layers] + \
         [l.feed_forward.w2.weight for l in m.layers] + [l.feed_forward.w3.weight for l in m.layers]
    out = a.out or os.path.join(HERE, "out", "lili", f"lili_{a.size}_q4.bin")
    with open(out, "wb") as f:
        f.write(struct.pack("<Ii", 0x616B3432, 3))
        f.write(struct.pack("<iiiiiii", p.dim, p.hidden_dim, p.n_layers, p.n_heads, p.n_heads, p.vocab_size, p.max_seq_len))
        f.write(struct.pack("<B", 1)); f.write(struct.pack("<i", GS)); f.write(b"\0" * (256 - f.tell()))
        for l in m.layers: f.write(l.attention_norm.weight.detach().float().numpy().astype("<f4").tobytes())
        for l in m.layers: f.write(l.ffn_norm.weight.detach().float().numpy().astype("<f4").tobytes())
        f.write(m.norm.weight.detach().float().numpy().astype("<f4").tobytes())
        for w in ws:
            nib, sc = q4(w); f.write(nib.tobytes()); f.write(sc.astype("<f2").tobytes())
        f.write(b"LSCH"); f.write(struct.pack("<III", S.VERSION, vocab_hash(), len(S.TOKENS)))
    print(f"export {a.size}: {out} {os.path.getsize(out) / 1e6:.2f} MB (schema v{S.VERSION}, vocab hash {vocab_hash():08x})")

if __name__ == "__main__":
    main()
