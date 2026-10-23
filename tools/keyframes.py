#!/usr/bin/env python3
"""Pixel Lili: animation keyframes by image EDITING with FLUX.2 klein 4B (WSL, 2026-10-23, the 2D line).

    (WSL, ~/dev/lili-venv)  python tools/keyframes.py encode   --set walk_SE
                            python tools/keyframes.py generate --set walk_SE [--variant plain|pixel|base|sheet] [--seed 1]

Not a video: every keyframe is its own edit of the SAME cleaned Lili sprite ("same dog, change only the legs:
..."), so nothing drifts from frame to frame. A set (SETS below) = a source pose + one instruction per keyframe.
Variants (all on the same embeddings):
  plain   FLUX.2 klein 4B (step-distilled: 4 steps, guidance 1.0)
  pixel   the same + the Limbicnation pixel-art LoRA (made for the distilled model)
  base    FLUX.2 klein base 4B (not distilled: 28 steps, real guidance with a negative prompt)
  sheet   the base model + the svntax 4-walk sprite-sheet LoRA: ONE 512 px image, a 4 x 4 grid (its own prompt)
Two processes: 'encode' runs only the Qwen3 text encoder (CPU, ~7.5 GB; the two klein models share it, the
file hashes are identical) and saves the embeddings; 'generate' loads the pipeline without it and streams the
transformer to the GPU block by block (7.2 GB of weights; the desktop keeps ~3 GB of the 8).
The sprite goes in 8x nearest on a flat green screen; frames -> assets/raw_renders/video/<set>_<variant>/f000.png
(scale.txt = 8), which tools/video_to_sprites.py turns back into 96 px sprites with Lili's colours.
"""
import argparse, os, sys, time
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = "black-forest-labs/FLUX.2-klein-4B"
REPO_BASE = "black-forest-labs/FLUX.2-klein-base-4B"
LORAS = {"pixel": ("Limbicnation/pixel-art-lora", "pytorch_lora_weights.safetensors"),
         "sheet": ("svntax-dev/pixel_spritesheet_4walk_small_lora_v1", "pixel_4walk_small_flux2_klein_base_4b_v1_000002750.safetensors")}
GREEN = (0, 177, 64)
SCALE, CELL = 8, 96

KEEP = ("Edit this pixel art game sprite. Keep exactly the same chihuahua puppy: the same face, the same eyes, the same "
        "colours, the same pixel art style with the dark outline, the same size and the same position, on the same flat "
        "green background. Her ears must stay exactly as in the input image: the ear on the left side of the picture is "
        "folded and droops down, the ear on the right side of the picture stands up straight. Change only this: ")
SHEET = ("Create a pixel art spritesheet of the character in the image, a small cream and white chihuahua puppy with one "
         "folded ear. The spritesheet is a 4 by 4 grid of four rows of frames - first row is 3 walking frames facing down "
         "and 1 frame sitting, second row is 3 walking frames facing left and 1 frame jumping left, third row is 3 walking "
         "frames facing right and 1 frame jumping right, fourth row is 3 walking frames back view facing up and 1 frame "
         "lying on floor.")
NEG = "blurry, smooth shading, photo, 3d render, extra legs, deformed, text, watermark"
SETS = {
    # the walk, side view facing right: a 4-beat trot, diagonal leg pairs (spec 7.4 A04)
    "walk_SE": ("walk_SE", [
        "a big trotting step: the front leg nearest to us is lifted and stretched far forward to the right, the hind leg nearest to us is stretched far back to the left, the two far legs are under the body",
        "the legs pass each other under the body: all four paws close together under the belly, the nearest front paw lifted off the ground, the body one pixel higher",
        "a big trotting step: the front leg nearest to us is stretched far back under the chest, the hind leg nearest to us is lifted and stretched forward under the belly, the two far legs reach forward and back",
        "the legs pass each other under the body: all four paws close together under the belly, the nearest hind paw lifted off the ground, the body one pixel higher",
    ]),
    # the tail wag, standing (A01): the curled tail swings, nothing else moves
    "wag_SW": ("stand_SW", [
        "the curled tail swings to the left",
        "the curled tail points straight up",
        "the curled tail swings to the right",
        "the curled tail points straight up",
    ]),
    # the svntax sheet: one prompt, one image
    "sheet_S": ("stand_S", None),
}

def out_dir(name): return os.path.join(ROOT, "assets", "raw_renders", "video", name)

def sprite_frame(pose, scale=SCALE, bg=GREEN):
    im = Image.open(os.path.join(ROOT, "assets", "sprites", "lili_src", pose + ".png"))
    idx = np.asarray(im); rgb = np.asarray(im.convert("RGB")).copy(); rgb[idx == 0] = bg
    return Image.fromarray(rgb).resize((CELL * scale, CELL * scale), Image.NEAREST)

def encode(a):
    import torch
    from transformers import Qwen2TokenizerFast, Qwen3ForCausalLM      # the classes model_index.json names
    from diffusers import Flux2KleinPipeline
    t = time.time()
    tok = Qwen2TokenizerFast.from_pretrained(REPO, subfolder="tokenizer")
    te = Qwen3ForCausalLM.from_pretrained(REPO, subfolder="text_encoder", torch_dtype=torch.bfloat16)
    pipe = Flux2KleinPipeline(tokenizer=tok, text_encoder=te, transformer=None, vae=None, scheduler=None)
    pose, steps = SETS[a.set]
    prompts = [SHEET] if steps is None else [KEEP + s for s in steps]
    def enc(p):
        pe = pipe.encode_prompt(p, device="cpu")
        return pe[0] if isinstance(pe, tuple) else pe
    with torch.no_grad():
        embeds = [enc(p) for p in prompts]
        neg = enc(NEG)
    os.makedirs(out_dir(a.set), exist_ok=True)
    torch.save({"pose": pose, "steps": steps or ["sheet"], "embeds": embeds, "neg": neg}, os.path.join(out_dir(a.set), "klein_embeds.pt"))
    print(f"encoded {len(prompts)} prompts + the negative in {time.time() - t:.0f} s: {tuple(embeds[0].shape)}")

def generate(a):
    import torch
    from diffusers import Flux2KleinPipeline
    t = time.time()
    emb = torch.load(os.path.join(out_dir(a.set), "klein_embeds.pt"))
    base = a.variant in ("base", "sheet")
    pipe = Flux2KleinPipeline.from_pretrained(REPO_BASE if base else REPO, text_encoder=None, tokenizer=None, torch_dtype=torch.bfloat16)
    if a.variant in LORAS:
        repo, weight = LORAS[a.variant]
        pipe.load_lora_weights(repo, weight_name=weight, adapter_name=a.variant)
        print(f"LoRA {repo} / {weight}")
    pipe.transformer.enable_group_offload(onload_device=torch.device("cuda"), offload_device=torch.device("cpu"),
                                          offload_type="block_level", num_blocks_per_group=2, use_stream=False)
    pipe.vae.to("cuda")
    sheet = a.variant == "sheet"
    scale = 4 if sheet else SCALE
    ref = sprite_frame(emb["pose"], scale, (255, 255, 255) if sheet else GREEN)
    W = H = 512 if sheet else CELL * SCALE
    d = out_dir(f"{a.set}_{a.variant}" + (f"_s{a.seed}" if a.seed != 1 else ""))
    os.makedirs(d, exist_ok=True)
    for f in os.listdir(d):
        if f.startswith("f") and f.endswith(".png"): os.remove(os.path.join(d, f))
    ref.save(os.path.join(d, "reference.png"))
    kw = dict(num_inference_steps=4, guidance_scale=1.0) if not base else dict(num_inference_steps=a.steps, guidance_scale=a.cfg)
    for i, pe in enumerate(emb["embeds"]):
        g = torch.Generator("cuda").manual_seed(a.seed + i)
        if base:
            kw["negative_prompt_embeds"] = emb["neg"].to("cuda")
        with torch.no_grad():
            img = pipe(image=[ref], prompt_embeds=pe.to("cuda"), height=H, width=W, generator=g, **kw).images[0]
        img.save(os.path.join(d, f"f{i:03d}.png"))
        print(f"  {a.variant} keyframe {i}: {emb['steps'][i][:60]}")
    open(os.path.join(d, "scale.txt"), "w").write(str(scale))
    print(f"generated {len(emb['embeds'])} images {W}x{H} ({a.variant}) in {time.time() - t:.0f} s, peak VRAM {torch.cuda.max_memory_allocated() / 2**30:.1f} GB -> {d}")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("phase", choices=["encode", "generate"])
    ap.add_argument("--set", required=True, choices=sorted(SETS))
    ap.add_argument("--variant", default="plain", choices=["plain", "pixel", "base", "sheet"])
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--steps", type=int, default=28)
    ap.add_argument("--cfg", type=float, default=4.0)
    a = ap.parse_args()
    encode(a) if a.phase == "encode" else generate(a)

if __name__ == "__main__":
    main()
