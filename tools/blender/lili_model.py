"""lili_model.py - B2-B4: Lili's model (spec 3.1 L1-L9, the approved character sheet
reference/concept/lili_sheet_approved_2026-10-21.jpg). Built by script so it is reproducible and
the owner can still open the .blend in Blender and look around.

    py -3.12 tools/blender_run.py --blend assets/blender/scene.blend tools/blender/lili_model.py -- --out assets/blender/lili.blend [--sheet docs/img/m3_4_lili_8dir.png]

The sheet's proportions, in metres (1 logical px = 1.25 cm, lili_common.PX_M): a big apple head
nearly as long as the body, big round dark eyes set apart, a short muzzle with the worn pinkish
nose, the white blaze up the forehead, white chest, belly and paws, short legs, the tail curled
high over the back. Her LEFT ear stands, her RIGHT ear droops (D-03): the drooping one is on
the viewer's left when she faces the camera. Nothing is mirrored anywhere in the pipeline.

The handles are the same names the placeholder has (lili_root, lili_head, lili_leg0..3,
lili_tail), so render_all.py's animations drive both. Colours come from the master palette.
"""
import argparse, json, math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
from mathutils import Vector
import lili_common as lc

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

def ramp_hex(name):
    """the master palette's 8 shades of a material, as hex (assets/palettes/master.json)"""
    m = json.load(open(os.path.join(ROOT, "assets", "palettes", "master.json"), encoding="utf-8"))
    i = m["ramps"].index(name)
    return ["#%02x%02x%02x" % tuple(m["rgb"][16 + i * 8 + s]) for s in range(8)], m["fixed"]

def materials():
    fur, fixed = ramp_hex("fur")
    white, _ = ramp_hex("fur_w")
    return {
        "fawn":  lc.toon_material("lili_fawn", fur[2], fur[4], fur[6]),
        "white": lc.toon_material("lili_white", white[3], white[5], white[7]),
        "eye":   lc.toon_material("lili_eye", fixed["eye"], fixed["eye"], fixed["eye"]),
        "nose":  lc.toon_material("lili_nose", fixed["nose"], fixed["nose"], fixed["ear_pink"]),
        "pink":  lc.toon_material("lili_ear_pink", fixed["nose"], fixed["ear_pink"], fixed["ear_pink"]),
        "gloss": lc.toon_material("lili_eye_gloss", "#fffaf2", "#fffaf2", "#fffaf2"),
    }

def sphere(name, loc, scale, mat, parent, rot=(0, 0, 0), coll="LILI", seg=32):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=seg, ring_count=seg // 2, location=loc)
    o = bpy.context.object
    o.name = name; o.scale = scale; o.rotation_euler = rot
    o.data.materials.append(mat)
    bpy.ops.object.shade_smooth()
    o.parent = parent
    lc.link(o, coll)
    return o

def capsule(name, loc, r, h, mat, parent, coll="LILI"):
    """a leg: a cylinder with a rounded foot"""
    bpy.ops.mesh.primitive_cylinder_add(vertices=16, radius=r, depth=h, location=(loc[0], loc[1], loc[2] - h / 2))
    o = bpy.context.object; o.name = name
    o.data.materials.append(mat); bpy.ops.object.shade_smooth()
    o.parent = parent; lc.link(o, coll)
    return o

def ear(name, loc, rot, mat, inner, parent, coll="LILI", h=0.13, w=0.075, t=0.018):
    """a big triangular chihuahua ear: a flattened cone, with a pink inner face"""
    bpy.ops.mesh.primitive_cone_add(vertices=4, radius1=1.0, radius2=0.08, depth=2.0, location=loc)
    o = bpy.context.object; o.name = name
    o.scale = (w / 2, t / 2, h / 2); o.rotation_euler = rot
    o.data.materials.append(mat); bpy.ops.object.shade_smooth()
    o.parent = parent; lc.link(o, coll)
    bpy.ops.mesh.primitive_cone_add(vertices=4, radius1=0.7, radius2=0.05, depth=1.6, location=(0, 0, 0))
    p = bpy.context.object; p.name = name + "_inner"
    p.data.materials.append(inner); bpy.ops.object.shade_smooth()
    p.parent = o; p.location = (0, -0.55, -0.05); p.scale = (1, 0.3, 1)      # pushed to the ear's front face (its -Y side)
    lc.link(p, coll)
    return o

def tail(parent, mat, coll="LILI"):
    """the tail curled high over the back (L8): a bevelled Bezier curve, thinner towards the tip"""
    cu = bpy.data.curves.new("lili_tail_curve", "CURVE")
    cu.dimensions = "3D"; cu.bevel_depth = 0.021; cu.bevel_resolution = 4; cu.fill_mode = "FULL"; cu.use_fill_caps = True
    sp = cu.splines.new("BEZIER"); sp.bezier_points.add(3)
    pts = [((0.0, 0.0, 0.0), 1.0), ((-0.05, 0.0, 0.08), 0.9), ((-0.01, 0.015, 0.15), 0.7), ((0.07, 0.03, 0.13), 0.45)]
    for bp, (co, r) in zip(sp.bezier_points, pts):
        bp.co = co; bp.radius = r; bp.handle_left_type = bp.handle_right_type = "AUTO"
    o = bpy.data.objects.new("lili_tail_mesh", cu)
    o.data.materials.append(mat)
    o.parent = parent
    lc.link(o, coll)
    return o

def build(coll="LILI"):
    M = materials()
    E = lambda name, loc, par: lc.link(_empty(name, loc, par), coll)
    root = E("lili_root", (0, 0, 0), None)
    # the body: an egg, fuller at the chest, tapering to the rump; low to the ground (short legs)
    body = sphere("lili_body", (-0.01, 0, 0.175), (0.125, 0.092, 0.082), M["fawn"], root)
    sphere("lili_rump", (-0.085, 0, 0.172), (0.080, 0.082, 0.072), M["fawn"], root)
    sphere("lili_chest", (0.05, 0, 0.145), (0.095, 0.078, 0.066), M["white"], root)        # white chest and belly (L3)
    sphere("lili_belly", (-0.04, 0, 0.125), (0.10, 0.070, 0.045), M["white"], root)
    # the head: big, round, forward and up; the neck joint is the animation handle
    head = E("lili_head", (0.11, 0, 0.305), root)
    sphere("lili_skull", (0.035, 0, 0.03), (0.105, 0.100, 0.098), M["fawn"], head)
    sphere("lili_cheek_l", (0.08, 0.055, -0.01), (0.045, 0.040, 0.040), M["fawn"], head)
    sphere("lili_cheek_r", (0.08, -0.055, -0.01), (0.045, 0.040, 0.040), M["fawn"], head)
    sphere("lili_muzzle", (0.115, 0, -0.015), (0.052, 0.046, 0.036), M["white"], head)      # the short muzzle, white
    sphere("lili_chin", (0.10, 0, -0.045), (0.035, 0.035, 0.022), M["white"], head)
    sphere("lili_nose", (0.163, 0, -0.002), (0.016, 0.018, 0.013), M["nose"], head)         # the worn pinkish-brown nose (L6)
    sphere("lili_blaze", (0.108, 0, 0.055), (0.034, 0.016, 0.070), M["white"], head, rot=(0, math.radians(-38), 0))   # the white blaze (L4)
    for side, y in (("l", 0.047), ("r", -0.047)):
        sphere(f"lili_eye_{side}", (0.112, y, 0.035), (0.026, 0.026, 0.028), M["eye"], head)   # big, round, dark, set apart (L5)
        sphere(f"lili_gloss_{side}", (0.133, y + (0.006 if side == "l" else -0.006), 0.046), (0.007, 0.007, 0.007), M["gloss"], head)
    ear("lili_ear_l", (0.015, 0.068, 0.155), (math.radians(-18), math.radians(-6), math.radians(8)), M["fawn"], M["pink"], head)      # her left ear: up, planted on the skull's top edge
    ear("lili_ear_r", (0.015, -0.095, 0.095), (math.radians(112), math.radians(0), math.radians(-20)), M["fawn"], M["pink"], head)   # her RIGHT ear droops outwards (D-03)
    # the legs: short; the fronts white, the backs fawn with white paws
    legs = []
    for i, (x, y) in enumerate(((0.065, 0.050), (0.065, -0.050), (-0.085, 0.052), (-0.085, -0.052))):
        hip = E(f"lili_leg{i}", (x, y, 0.115), root)
        capsule(f"lili_leg{i}_mesh", (0, 0, 0.01), 0.022, 0.11, M["white"] if x > 0 else M["fawn"], hip)
        sphere(f"lili_paw{i}", (0.012, 0, -0.098), (0.030, 0.025, 0.016), M["white"], hip, seg=16)
        legs.append(hip)
    t = E("lili_tail", (-0.14, 0, 0.215), root)
    tail(t, M["fawn"])
    return {"root": root, "body": body, "head": head, "legs": legs, "tail": t}

def _empty(name, loc, parent):
    e = bpy.data.objects.new(name, None); e.location = loc; e.parent = parent
    bpy.context.scene.collection.objects.link(e)
    return e

def handles_from_scene():
    """the same dict from an already built model (render_all.py on lili.blend)"""
    o = bpy.data.objects
    return {"root": o["lili_root"], "body": o.get("lili_body"), "head": o["lili_head"], "legs": [o[f"lili_leg{i}"] for i in range(4)], "tail": o["lili_tail"]}

if __name__ == "__main__":
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--sheet")
    a = ap.parse_args(argv)
    sc = bpy.context.scene
    for n in lc.COLLECTIONS:
        lc.collection(n)
    for o in list(lc.collection("LILI").objects) + list(lc.collection("CHECK").objects):      # rebuild from scratch
        bpy.data.objects.remove(o, do_unlink=True)
    lc.world_black(sc); lc.sun()
    h = build("LILI")
    for c in lc.COLLECTIONS:
        lc.collection(c).hide_render = c not in ("LILI", "CAMERA_LIGHT")
    lc.camera(sc, "cell")
    # the size check: the standing dog must fit the 48 px cell with room for the head (spec 7.3: ~40 x 30 logical px)
    bpy.context.view_layer.update()
    xs, ys = [], []
    for o in lc.collection("LILI").objects:
        if o.type in ("MESH", "CURVE"):
            for v in o.bound_box:
                p = lc.to_logical(sc, o.matrix_world @ Vector(v)); xs.append(p[0]); ys.append(p[1])
    print(f"lili: the model spans logical x {min(xs):.1f}..{max(xs):.1f}, y {min(ys):.1f}..{max(ys):.1f} in the cell (facing east; pivot at (24, 40))")
    if a.sheet:
        d = os.path.join(ROOT, "assets", "raw_renders", "_lili8")
        os.makedirs(d, exist_ok=True)
        for di, name in enumerate(lc.DIR_NAMES):
            h["root"].rotation_euler = (0, 0, math.radians(lc.heading_deg(di)))
            sc.render.filepath = os.path.join(d, f"dir_{di}_{name}.png")
            bpy.ops.render.render(write_still=True)
        h["root"].rotation_euler = (0, 0, 0)
        print(f"lili: 8 directions rendered -> {d}")
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(a.out), compress=True)
    print(f"lili: model saved -> {a.out}")
