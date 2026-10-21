#!/usr/bin/env python3
"""Pixel Lili: the whole asset line in one command (spec 7.5, M3.3). Windows side (Blender lives here).

    py -3.12 tools/build_assets.py              everything: palette -> palette_gen.c -> scene -> renders -> sprites -> sheets -> pak
    py -3.12 tools/build_assets.py --from pixelize    start at a step: palette | scene | model | render | pixelize | pack
    py -3.12 tools/build_assets.py --anim walk        only one animation through render and pixelize

Then build and check the simulator in WSL (`make LILI=1 check` in sim/): it loads assets/build/assets.pak.
Hand-fixed frames in assets/sprites/overrides/<anim>/ survive every run (pixelize.py rule 5).
"""
import argparse, os, subprocess, sys, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PY = sys.executable
STEPS = ["palette", "scene", "model", "render", "pixelize", "pack"]

def run(label, cmd):
    t = time.time()
    print(f"== {label}")
    subprocess.check_call(cmd, cwd=ROOT)
    print(f"   ({time.time() - t:.1f} s)")

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--from", dest="start", choices=STEPS, default="palette")
    ap.add_argument("--anim", default="all")
    a = ap.parse_args()
    todo = STEPS[STEPS.index(a.start):]
    if "palette" in todo:
        run("palette from the concept art", [PY, "tools/palette_from_photos.py"])
        run("palette_gen.c", [PY, "tools/gen_palette.py"])
    if "scene" in todo:
        run("Blender scene", [PY, "tools/blender_run.py", "tools/blender/scene_setup.py", "--", "--out", "assets/blender/scene.blend"])
    if "model" in todo:
        run("Lili's model (lili.blend + the 8-direction sheet)", [PY, "tools/blender_run.py", "--blend", "assets/blender/scene.blend", "tools/blender/lili_model.py", "--",
                                                                   "--out", "assets/blender/lili.blend", "--sheet", "docs/img/m3_4_lili_8dir.png"])
        run("the 8-direction sheet", [PY, "tools/contact_sheet.py", "assets/raw_renders/_lili8", "docs/img/m3_4_lili_8dir.png", "--cols", "4"])
    if "render" in todo:
        blend = "assets/blender/lili.blend" if os.path.exists(os.path.join(ROOT, "assets", "blender", "lili.blend")) else "assets/blender/scene.blend"
        run(f"render from {blend}", [PY, "tools/blender_run.py", "--blend", blend, "tools/blender/render_all.py", "--", "--anim", a.anim])
    if "pixelize" in todo:
        run("pixelize + contact sheets", [PY, "tools/pixelize.py", "--anim", a.anim])
    if "pack" in todo:
        run("pack", [PY, "tools/pack_assets.py"])
    print("assets: done. Next: in WSL, sim/: make LILI=1 check")

if __name__ == "__main__":
    main()
