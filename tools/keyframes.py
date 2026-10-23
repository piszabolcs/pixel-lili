#!/usr/bin/env python3
"""Pixel Lili: animation keyframes by image EDITING with FLUX.2 klein 4B (WSL, 2026-10-23, the 2D line).

    (WSL, ~/dev/lili-venv)  python tools/keyframes.py encode   --set walk_SE
                            python tools/keyframes.py generate --set walk_SE [--seed 1]

Not a video: every keyframe is its own edit of the SAME cleaned Lili sprite ("same dog, change only the legs:
..."), so nothing drifts from frame to frame. A set (SETS below) = a source pose + one instruction per keyframe.
Two processes, as for the video model: 'encode' runs only the Qwen3 text encoder (CPU, ~7.5 GB) and saves the
embeddings; 'generate' loads the pipeline without it and streams the 4B transformer to the GPU block by block
(7.2 GB of weights, the desktop keeps ~3 GB of the 8). klein is step-distilled: 4 steps, guidance 1.0.
The sprite goes in 8x nearest on a flat green screen; frames -> assets/raw_renders/video/<set>/f000.png ...
(scale.txt = 8), which tools/video_to_sprites.py turns back into 96 px sprites with Lili's colours.
"""
import argparse, os, sys, time
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = "black-forest-labs/FLUX.2-klein-4B"
GREEN = (0, 177, 64)
SCALE, CELL = 8, 96

KEEP = ("Edit this pixel art game sprite. Keep exactly the same chihuahua puppy: the same face, eyes, ears (her right "
        "ear folded, her left ear standing), the same colours, the same pixel art style with the dark outline, the same "
        "size and the same position, on the same flat green background. Change only this: ")
SETS = {
    # the walk, side view facing right: a 4-beat trot, diagonal leg pairs (spec 7.4 A04)
    "walk_SE": ("walk_SE", [
        "the front leg nearest to us reaches far forward and the hind leg farthest from us reaches forward, the other two legs push back, a trotting step",
        "all four legs pass under the body, close together, the body a little higher, mid stride",
        "the front leg farthest from us reaches far forward and the hind leg nearest to us reaches forward, the other two legs push back, a trotting step",
        "all four legs pass under the body, close together, the body a little higher, mid stride",
    ]),
    # the tail wag, standing (A01): the curled tail swings, nothing else moves
    "wag_SW": ("stand_SW", [
        "the curled tail swings to the left",
        "the curled tail points straight up",
        "the curled tail swings to the right",
        "the curled tail points straight up",
    ]),
}

def out_dir(name): return os.path.join(ROOT, "assets", "raw_renders", "video", name)

def sprite_frame(pose):
    im = Image.open(os.path.join(ROOT, "assets", "sprites", "lili_src", pose + ".png"))
    idx = np.asarray(im); rgb = np.asarray(im.convert("RGB")).copy(); rgb[idx == 0] = GREEN
    return Image.fromarray(rgb).resize((CELL * SCALE, CELL * SCALE), Image.NEAREST)

def encode(a):
    import torch
    from transformers import Qwen2TokenizerFast, Qwen3ForCausalLM      # the classes model_index.json names
    from diffusers import Flux2KleinPipeline
    t = time.time()
    tok = Qwen2TokenizerFast.from_pretrained(REPO, subfolder="tokenizer")
    te = Qwen3ForCausalLM.from_pretrained(REPO, subfolder="text_encoder", torch_dtype=torch.bfloat16)
    pipe = Flux2KleinPipeline(tokenizer=tok, text_encoder=te, transformer=None, vae=None, scheduler=None)
    pose, steps = SETS[a.set]
    embeds = []
    with torch.no_grad():
        for s in steps:
            pe = pipe.encode_prompt(KEEP + s, device="cpu")
            embeds.append(pe[0] if isinstance(pe, tuple) else pe)
    os.makedirs(out_dir(a.set), exist_ok=True)
    torch.save({"pose": pose, "steps": steps, "embeds": embeds}, os.path.join(out_dir(a.set), "klein_embeds.pt"))
    print(f"encoded {len(steps)} prompts in {time.time() - t:.0f} s: {tuple(embeds[0].shape)}")

def generate(a):
    import torch
    from diffusers import Flux2KleinPipeline
    t = time.time()
    d = out_dir(a.set)
    emb = torch.load(os.path.join(d, "klein_embeds.pt"))
    pipe = Flux2KleinPipeline.from_pretrained(REPO, text_encoder=None, tokenizer=None, torch_dtype=torch.bfloat16)
    pipe.transformer.enable_group_offload(onload_device=torch.device("cuda"), offload_device=torch.device("cpu"),
                                          offload_type="block_level", num_blocks_per_group=2, use_stream=False)
    pipe.vae.to("cuda")
    ref = sprite_frame(emb["pose"])
    W = H = CELL * SCALE
    for f in os.listdir(d):
        if f.startswith("f") and f.endswith(".png"): os.remove(os.path.join(d, f))
    ref.save(os.path.join(d, "reference.png"))
    for i, pe in enumerate(emb["embeds"]):
        g = torch.Generator("cuda").manual_seed(a.seed + i)
        with torch.no_grad():
            img = pipe(image=[ref], prompt_embeds=pe.to("cuda"), height=H, width=W, num_inference_steps=4,
                       guidance_scale=1.0, generator=g).images[0]
        img.save(os.path.join(d, f"f{i:03d}.png"))
        print(f"  keyframe {i}: {emb['steps'][i][:60]}...")
    open(os.path.join(d, "scale.txt"), "w").write(str(SCALE))
    print(f"generated {len(emb['embeds'])} keyframes {W}x{H} in {time.time() - t:.0f} s, peak VRAM {torch.cuda.max_memory_allocated() / 2**30:.1f} GB -> {d}")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("phase", choices=["encode", "generate"])
    ap.add_argument("--set", required=True, choices=sorted(SETS))
    ap.add_argument("--seed", type=int, default=1)
    a = ap.parse_args()
    encode(a) if a.phase == "encode" else generate(a)

if __name__ == "__main__":
    main()
