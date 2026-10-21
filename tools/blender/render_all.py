"""render_all.py - B6 of the Blender line (spec 7.5): every frame of every animation in every
direction, RGBA, 4x (192 x 192), to assets/raw_renders/<anim>/d<dir>_f<frame>.png.

    py -3.12 tools/blender_run.py --blend assets/blender/scene.blend tools/blender/render_all.py -- [--anim walk] [--out assets/raw_renders]

The catalogue is assets/anims.json. The dog: whatever stands in the LILI collection of the .blend given (assets/blender/lili.blend
from lili_model.py, M3.4), else the placeholder (lili_rig_placeholder.py) is built into scene.blend. Directions are the game's dir8_t
(S first, clockwise), made by turning the dog, never the camera or the light (spec 7.3).
A manifest.json per animation records what was rendered (the pixelizer and the packer read it).
"""
import argparse, json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
import lili_common as lc
import lili_rig_placeholder as rig

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ap = argparse.ArgumentParser()
ap.add_argument("--anim", default="all")
ap.add_argument("--out", default="assets/raw_renders")
a = ap.parse_args(argv)

cat = {k: v for k, v in json.load(open(os.path.join(ROOT, "assets", "anims.json"), encoding="utf-8")).items() if not k.startswith("_")}
names = list(cat) if a.anim == "all" else [a.anim]
sc = bpy.context.scene
for n in lc.COLLECTIONS:
    lc.collection(n)
lc.world_black(sc)
lc.sun()
lc.camera(sc, "cell")

# the dog: whatever is in the LILI collection, else the placeholder
lili_coll = lc.collection("LILI")
import lili_model
if not lili_coll.objects:
    h = rig.build("LILI")
    print("lili: placeholder dog (no model in the LILI collection yet)")
else:
    h = lili_model.handles_from_scene()                                     # lili.blend (M3.4): the same handle names
    print("lili: Lili's model from the LILI collection")
anims = rig.ANIMS
for c in lc.COLLECTIONS:                                                   # only the dog renders in a sprite cell
    lc.collection(c).hide_render = c not in ("LILI", "CAMERA_LIGHT")

total = 0
for name in names:
    spec = cat[name]
    dirs = list(range(8)) if spec["dirs"] == 8 else [lc.DIR_NAMES.index(d) for d in spec["dirs"]]
    out = os.path.join(ROOT, a.out, name)
    os.makedirs(out, exist_ok=True)
    frames = []
    for d in dirs:
        for f in range(spec["frames"]):
            anims[name](h, f / spec["frames"])
            h["root"].rotation_euler = (0, 0, __import__("math").radians(lc.heading_deg(d)))
            fn = f"d{d}_f{f}.png"
            sc.render.filepath = os.path.join(out, fn)
            bpy.ops.render.render(write_still=True)
            frames.append({"dir": d, "frame": f, "file": fn, "mouth": None, "head": None})
            total += 1
    json.dump({"anim": name, "id": spec["id"], "fps": spec["fps"], "loop": spec["loop"], "dirs": dirs, "frames": spec["frames"],
               "cell": lc.CELL, "scale": lc.RS, "pivot": list(lc.PIVOT), "files": frames},
              open(os.path.join(out, "manifest.json"), "w", encoding="utf-8"), indent=1)
    print(f"lili: {name}: {len(dirs)} dirs x {spec['frames']} frames -> {os.path.relpath(out, ROOT)}")
print(f"lili: rendered {total} frames at {sc.render.resolution_x} x {sc.render.resolution_y}")
