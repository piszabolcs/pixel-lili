"""scene_setup.py - B1 of the Blender line (spec 7.5, M3.1): the empty production scene.

    py -3.12 tools/blender_run.py tools/blender/scene_setup.py -- --out assets/blender/scene.blend --check-dir assets/raw_renders/_check

Builds, from a factory-empty Blender: the collections every later asset lives in, the one
camera (orthographic, 35 degrees, lili_common.py), the sun from the upper left, a black
world with no ambient (the toon ramp alone decides the tones), EEVEE with exact colours.
Then it proves the camera numerically (world points -> the logical px the formulas promise,
for the sprite cell and for the whole room) and, with --check-dir, renders a placeholder
chihuahua in the eight game directions so the light, the toon bands and the scale can be
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
depth_66 = (lc.LOG - 66) * lc.PX_M / SIN                    # room.json: the floor starts at logical y 66
expect("room: the world origin", (0, 0, 0), (0, lc.LOG))
expect("room: 3 m to the right", (3.0, 0, 0), (lc.LOG, lc.LOG))
expect(f"room: the floor's back edge ({depth_66:.2f} m)", (1.5, depth_66, 0), (120, 66))
print(f"lili: the room is {lc.LOG * lc.PX_M:.2f} m wide and {depth_66:.2f} m deep to the wall; Lili's 40 px = {40 * lc.PX_M * 100:.0f} cm")

# ---- a placeholder chihuahua (the M3.3 chain uses it until B2's model exists) ----
def prim(kind, loc, scale, mat, parent, rot=(0, 0, 0)):
    if kind == "s":
        bpy.ops.mesh.primitive_uv_sphere_add(segments=24, ring_count=12, location=loc)
    elif kind == "c":
        bpy.ops.mesh.primitive_cylinder_add(vertices=12, location=loc)
    else:
        bpy.ops.mesh.primitive_cone_add(vertices=10, location=loc)
    o = bpy.context.object
    o.scale, o.rotation_euler = scale, rot
    o.data.materials.append(mat)
    bpy.ops.object.shade_smooth()
    o.parent = parent
    lc.link(o, "CHECK")
    return o

if a.check_dir:
    FAWN = lc.toon_material("fawn", "#9c6534", "#d99a5b", "#f2c58e")      # placeholders until M3.2's master palette
    WHITE = lc.toon_material("white", "#b9aa9a", "#ece2d4", "#fffaf2")
    DARK = lc.toon_material("dark", "#120c0c", "#1c1618", "#3a3034")
    root = bpy.data.objects.new("lili_root", None)
    lc.link(root, "CHECK")
    prim("s", (0.00, 0, 0.17), (0.16, 0.09, 0.085), FAWN, root)             # body: ~32 cm nose to tail with the head
    prim("s", (0.05, 0, 0.14), (0.11, 0.07, 0.06), WHITE, root)             # chest and belly
    prim("s", (0.17, 0, 0.27), (0.075, 0.075, 0.07), FAWN, root)            # the apple head
    prim("s", (0.23, 0, 0.245), (0.04, 0.04, 0.03), WHITE, root)            # muzzle
    prim("s", (0.266, 0, 0.252), (0.011, 0.013, 0.01), DARK, root)          # nose
    for y in (0.03, -0.03):
        prim("s", (0.225, y, 0.285), (0.012, 0.012, 0.014), DARK, root)     # eyes
    prim("k", (0.15, 0.05, 0.345), (0.032, 0.014, 0.04), FAWN, root, rot=(math.radians(-20), 0, 0))      # her left ear: up
    prim("k", (0.15, -0.075, 0.31), (0.032, 0.014, 0.04), FAWN, root, rot=(math.radians(75), 0, 0))    # her RIGHT ear droops (D-03)
    for x in (0.09, -0.09):
        for y in (0.045, -0.045):
            prim("c", (x, y, 0.06), (0.018, 0.018, 0.06), WHITE if x > 0 else FAWN, root)
    prim("k", (-0.17, 0, 0.24), (0.018, 0.018, 0.07), FAWN, root, rot=(0, math.radians(-35), 0))       # tail, up
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
