#!/usr/bin/env python3
"""Pixel Lili: image-to-video on a Lili sprite with a local Wan model (WSL, the RTX 5060's 8 GB).

    (WSL, ~/dev/lili-venv)  python tools/video_anim.py encode --tag walk_SE --prompt "..."
                            python tools/video_anim.py generate --tag walk_SE --pose walk_SE [--frames 33 --steps 30 --seed 1]

Two processes on purpose: the umT5-XXL text encoder alone is ~11 GB, the WSL has 15 GB of RAM. 'encode'
loads only the encoder (CPU), writes the prompt embeddings; 'generate' loads the pipeline WITHOUT the
encoder (the 1.3B VACE transformer fits the GPU) and conditions the video on the sprite as its first frame
(VACE: frame 0 given and kept by a black mask, the rest grey and generated). The sprite is shown 5x nearest
on a flat green screen, so tools/video_to_sprites.py can key it out and resample it back onto the 96 grid.
Frames -> assets/raw_renders/video/<tag>/f000.png ...
"""
import argparse, gc, os, sys, time
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = os.environ.get("LILI_VIDEO_REPO", "Wan-AI/Wan2.1-VACE-1.3B-diffusers")
GREEN = (0, 177, 64)
SCALE, CELL = 5, 96
NEG = ("camera movement, zoom, pan, blurry, smeared, deformed, extra legs, extra tail, missing ears, "
       "changing colours, text, watermark, background change, 3d render, photo")

def out_dir(tag): return os.path.join(ROOT, "assets", "raw_renders", "video", tag)

def encode(a):
    import torch
    from transformers import AutoTokenizer, UMT5EncoderModel
    from diffusers import WanVACEPipeline
    t = time.time()
    tok = AutoTokenizer.from_pretrained(REPO, subfolder="tokenizer")
    te = UMT5EncoderModel.from_pretrained(REPO, subfolder="text_encoder", torch_dtype=torch.bfloat16)
    # the pipeline's own encode_prompt, with only the tokenizer and the encoder attached
    pipe = WanVACEPipeline(tokenizer=tok, text_encoder=te, transformer=None, vae=None, scheduler=None)
    with torch.no_grad():
        pe, ne = pipe.encode_prompt(a.prompt, NEG, do_classifier_free_guidance=True, device="cpu", dtype=torch.bfloat16)
    os.makedirs(out_dir(a.tag), exist_ok=True)
    torch.save({"prompt": a.prompt, "pe": pe, "ne": ne}, os.path.join(out_dir(a.tag), "embeds.pt"))
    print(f"encoded in {time.time() - t:.0f} s: {tuple(pe.shape)}")

def sprite_frame(pose):
    im = Image.open(os.path.join(ROOT, "assets", "sprites", "lili_src", pose + ".png"))
    idx = np.asarray(im)
    rgb = np.asarray(im.convert("RGB")).copy()
    rgb[idx == 0] = GREEN
    return Image.fromarray(rgb).resize((CELL * SCALE, CELL * SCALE), Image.NEAREST)

def generate(a):
    import torch
    from diffusers import WanVACEPipeline, AutoencoderKLWan
    t = time.time()
    emb = torch.load(os.path.join(out_dir(a.tag), "embeds.pt"))
    vae = AutoencoderKLWan.from_pretrained(REPO, subfolder="vae", torch_dtype=torch.float32)
    pipe = WanVACEPipeline.from_pretrained(REPO, vae=vae, text_encoder=None, tokenizer=None, torch_dtype=torch.bfloat16)
    pipe.to("cuda")
    pipe.vae.enable_tiling() if hasattr(pipe.vae, "enable_tiling") else None
    first = sprite_frame(a.pose)
    W = H = CELL * SCALE
    grey = Image.new("RGB", (W, H), (128, 128, 128))
    video = [first] + [grey] * (a.frames - 1)
    mask = [Image.new("L", (W, H), 0)] + [Image.new("L", (W, H), 255)] * (a.frames - 1)
    g = torch.Generator("cuda").manual_seed(a.seed)
    with torch.no_grad():
        frames = pipe(video=video, mask=mask, height=H, width=W, num_frames=a.frames, num_inference_steps=a.steps,
                      guidance_scale=a.cfg, prompt_embeds=emb["pe"].to("cuda"), negative_prompt_embeds=emb["ne"].to("cuda"),
                      generator=g, output_type="np").frames[0]
    d = out_dir(a.tag)
    for f in os.listdir(d):
        if f.startswith("f") and f.endswith(".png"): os.remove(os.path.join(d, f))
    for i, fr in enumerate(frames):
        Image.fromarray((np.clip(fr, 0, 1) * 255).astype(np.uint8)).save(os.path.join(d, f"f{i:03d}.png"))
    print(f"generated {len(frames)} frames {W}x{H} in {time.time() - t:.0f} s, peak VRAM {torch.cuda.max_memory_allocated() / 2**30:.1f} GB -> {d}")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("phase", choices=["encode", "generate"])
    ap.add_argument("--tag", required=True)
    ap.add_argument("--prompt")
    ap.add_argument("--pose")
    ap.add_argument("--frames", type=int, default=33)
    ap.add_argument("--steps", type=int, default=30)
    ap.add_argument("--cfg", type=float, default=5.0)
    ap.add_argument("--seed", type=int, default=1)
    a = ap.parse_args()
    encode(a) if a.phase == "encode" else generate(a)

if __name__ == "__main__":
    main()
