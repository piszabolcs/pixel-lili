"""Pixel Lili's brain model (spec 10.9): a small llama-style transformer, layer for layer the llama2.c architecture
the device's C engine runs (common/llm/q4_model.c): RMSNorm (eps 1e-5), RoPE (theta 10000, adjacent pairs),
multi-head causal attention, a SwiGLU feed-forward, the classifier tied to the embedding. The module names are
llama2.c's, so model/export_lili_q4.py writes the engine's "version 3" file without a translation table.

    S: dim 256, 6 layers, 8 heads, hidden 768  (~5.1M)
    M: dim 320, 8 layers, 8 heads, hidden 864  (~8.8M)
    L: dim 384, 8 layers, 8 heads, hidden 1024 (~14.3M)
"""
import math
from dataclasses import dataclass
import torch
import torch.nn as nn
import torch.nn.functional as F

SIZES = {"S": (256, 6, 8, 768), "M": (320, 8, 8, 864), "L": (384, 8, 8, 1024)}

@dataclass
class ModelArgs:
    dim: int = 256
    n_layers: int = 6
    n_heads: int = 8
    n_kv_heads: int = None
    vocab_size: int = 256
    hidden_dim: int = 768
    norm_eps: float = 1e-5
    max_seq_len: int = 48
    dropout: float = 0.0

class RMSNorm(nn.Module):
    def __init__(self, dim, eps):
        super().__init__(); self.eps = eps; self.weight = nn.Parameter(torch.ones(dim))
    def forward(self, x):
        return (x.float() * torch.rsqrt(x.float().pow(2).mean(-1, keepdim=True) + self.eps)).type_as(x) * self.weight

def rope_tables(head_dim, end, theta=10000.0):
    freqs = 1.0 / (theta ** (torch.arange(0, head_dim, 2).float() / head_dim))
    t = torch.arange(end).float()
    f = torch.outer(t, freqs)
    return torch.cos(f), torch.sin(f)

def apply_rope(x, cos, sin):                          # x: (B, T, H, D), pairs (2i, 2i+1) rotated by pos * freq_i
    xr, xi = x.float().reshape(*x.shape[:-1], -1, 2).unbind(-1)
    c, s = cos[None, :, None, :], sin[None, :, None, :]
    return torch.stack((xr * c - xi * s, xr * s + xi * c), dim=-1).flatten(3).type_as(x)

class Attention(nn.Module):
    def __init__(self, a):
        super().__init__()
        self.n_heads, self.head_dim = a.n_heads, a.dim // a.n_heads
        self.wq = nn.Linear(a.dim, a.dim, bias=False); self.wk = nn.Linear(a.dim, a.dim, bias=False)
        self.wv = nn.Linear(a.dim, a.dim, bias=False); self.wo = nn.Linear(a.dim, a.dim, bias=False)
        self.dropout = a.dropout
    def forward(self, x, cos, sin):
        B, T, _ = x.shape
        q = self.wq(x).view(B, T, self.n_heads, self.head_dim); k = self.wk(x).view(B, T, self.n_heads, self.head_dim)
        v = self.wv(x).view(B, T, self.n_heads, self.head_dim)
        q, k = apply_rope(q, cos, sin), apply_rope(k, cos, sin)
        y = F.scaled_dot_product_attention(q.transpose(1, 2), k.transpose(1, 2), v.transpose(1, 2), is_causal=True,
                                           dropout_p=self.dropout if self.training else 0.0)
        return self.wo(y.transpose(1, 2).reshape(B, T, -1))

class FeedForward(nn.Module):
    def __init__(self, a):
        super().__init__()
        self.w1 = nn.Linear(a.dim, a.hidden_dim, bias=False); self.w2 = nn.Linear(a.hidden_dim, a.dim, bias=False)
        self.w3 = nn.Linear(a.dim, a.hidden_dim, bias=False); self.drop = nn.Dropout(a.dropout)
    def forward(self, x):
        return self.drop(self.w2(F.silu(self.w1(x)) * self.w3(x)))

class Block(nn.Module):
    def __init__(self, a):
        super().__init__()
        self.attention = Attention(a); self.feed_forward = FeedForward(a)
        self.attention_norm = RMSNorm(a.dim, a.norm_eps); self.ffn_norm = RMSNorm(a.dim, a.norm_eps)
    def forward(self, x, cos, sin):
        x = x + self.attention(self.attention_norm(x), cos, sin)
        return x + self.feed_forward(self.ffn_norm(x))

class Transformer(nn.Module):
    def __init__(self, a: ModelArgs):
        super().__init__()
        self.params = a
        self.tok_embeddings = nn.Embedding(a.vocab_size, a.dim)
        self.drop = nn.Dropout(a.dropout)
        self.layers = nn.ModuleList(Block(a) for _ in range(a.n_layers))
        self.norm = RMSNorm(a.dim, a.norm_eps)
        self.output = nn.Linear(a.dim, a.vocab_size, bias=False)
        self.output.weight = self.tok_embeddings.weight               # tied (the export writes it once)
        cos, sin = rope_tables(a.dim // a.n_heads, a.max_seq_len)
        self.register_buffer("cos", cos, persistent=False); self.register_buffer("sin", sin, persistent=False)
        self.apply(self._init)
        for n, p in self.named_parameters():
            if n.endswith("wo.weight") or n.endswith("w2.weight"): nn.init.normal_(p, 0.0, 0.02 / math.sqrt(2 * a.n_layers))
    @staticmethod
    def _init(m):
        if isinstance(m, (nn.Linear, nn.Embedding)): nn.init.normal_(m.weight, 0.0, 0.02)
    def forward(self, tokens):
        T = tokens.shape[1]
        h = self.drop(self.tok_embeddings(tokens))
        for layer in self.layers: h = layer(h, self.cos[:T], self.sin[:T])
        return self.output(self.norm(h))

def make(size, dropout=0.0):
    d, l, h, hid = SIZES[size]
    return Transformer(ModelArgs(dim=d, n_layers=l, n_heads=h, hidden_dim=hid, dropout=dropout))
