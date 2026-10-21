"""scene_setup.py - B1 of the Blender line (spec 7.5, M3.1): the empty production scene.

    py -3.12 tools/blender_run.py tools/blender/scene_setup.py -- --out assets/blender/scene.blend --check-dir assets/raw_renders/_check

Builds, from a factory-empty Blender: the collections every later asset lives in, the one
camera (orthographic, 35 degrees, lili_common.py), the sun from the upper left, a black
world with no ambient (the toon ramp alone decides the tones), EEVEE with exact colours.
Then it proves the camera numerically (world points -> the logical px the formulas promise,
for the sprite cell and for the whole room) and, with --check-dir, renders a placeholder
chihuahua (lili_rig_placeholder.py) in the eight game directions so the light, the toon bands and the scale can be
looked at (tools/contact_sheet.py puts them side by side). Exit code 1 if a check fails.
"""
import argparse, math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
import lili_common as lc

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ap = argparse.ArgumentParser()
ap.add_argument("--out", required=True)
ap.add_argument("--check-dir")
a = ap.parse_args(argv)

bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
for n in lc.COLLECTIONS:
    lc.collection(n)
lc.world_black(sc)
lc.sun()

# ---- the camera, proved: the cell, then the room ----
SIN, COS = math.sin(math.radians(lc.ELEVATION_DEG)), math.cos(math.radians(lc.ELEVATION_DEG))
fails = 0
def expect(name, p, want):
    global fails
    got = lc.to_logical(sc, p)
    ok = abs(got[0] - want[0]) < 0.01 and abs(got[1] - want[1]) < 0.01
    fails += not ok
    print(f"lili: {'ok  ' if ok else 'FAIL'} {name:<34} world {p} -> logical ({got[0]:7.3f}, {got[1]:7.3f}), want ({want[0]:7.3f}, {want[1]:7.3f})")

lc.camera(sc, "cell")
px = lambda m: m / lc.PX_M
expect("cell: the pivot (paws' centre)", (0, 0, 0), lc.PIVOT)
expect("cell: 20 cm to the right", (0.2, 0, 0), (lc.PIVOT[0] + px(0.2), lc.PIVOT[1]))
expect("cell: 20 cm deeper (x sin 35)", (0, 0.2, 0), (lc.PIVOT[0], lc.PIVOT[1] - px(0.2) * SIN))
expect("cell: 20 cm up (x cos 35)", (0, 0, 0.2), (lc.PIVOT[0], lc.PIVOT[1] - px(0.2) * COS))
lc.camera(sc, "room")
depth_66 = (lc.LOG - 66 * lc.K) * lc.PX_M / SIN             # room.json: the floor starts at design y 66 (logical 66 x K)
expect("room: the world origin", (0, 0, 0), (0, lc.LOG))
expect("room: 3 m to the right", (3.0, 0, 0), (lc.LOG, lc.LOG))
expect(f"room: the floor's back edge ({depth_66:.2f} m)", (1.5, depth_66, 0), (lc.LOG / 2, 66 * lc.K))
print(f"lili: the room is {lc.LOG * lc.PX_M:.2f} m wide and {depth_66:.2f} m deep to the wall; Lili's 80 px = {80 * lc.PX_M * 100:.0f} cm")

# ---- the placeholder chihuahua (lili_rig_placeholder.py; the M3.3 chain renders it until B2's model exists) ----
import lili_rig_placeholder as rig
if a.check_dir:
    h = rig.build("CHECK")
    root = h["root"]
    lc.camera(sc, "cell")
    os.makedirs(a.check_dir, exist_ok=True)
    for d, name in enumerate(lc.DIR_NAMES):
        root.rotation_euler = (0, 0, math.radians(lc.heading_deg(d)))
        sc.render.filepath = os.path.join(os.path.abspath(a.check_dir), f"dir_{d}_{name}.png")
        bpy.ops.render.render(write_still=True)
    print(f"lili: 8 directions rendered ({sc.render.resolution_x} x {sc.render.resolution_y}) -> {a.check_dir}")

lc.camera(sc, "cell")
os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(a.out), compress=True)
print(f"lili: scene saved -> {a.out} | engine {sc.render.engine} | checks failed: {fails}")
if fails:
    sys.exit(1)
