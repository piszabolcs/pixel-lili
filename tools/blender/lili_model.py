"""lili_model.py - B2-B4: Lili's model (spec 3.1 L1-L9, the approved character sheet
reference/concept/lili_sheet_approved_2026-10-21.jpg). Built by script so it is reproducible and
the owner can still open the .blend in Blender and look around. The fawn body is ONE metaball
family (2026-10-21, the owner: "not balls, one continuous body"): body, neck, head, cheeks, muzzle
and legs blend into a single smooth surface; the white markings, eyes, nose, ears and tail sit on it.

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

def body_material(fur, white):
    """the fawn body with its white markings painted ON the surface (chest, belly, muzzle, blaze, paws):
    one toon material whose colour ramp is chosen by the surface point's position in the dog's own
    space (Object coordinates of the metaball base = root space, the dog facing +X). So the markings
    follow the smooth metaball surface instead of being separate shapes (the owner, 2026-10-21)."""
    m = bpy.data.materials.get("lili_body") or bpy.data.materials.new("lili_body")
    m.use_nodes = True
    nt = m.node_tree; nt.nodes.clear()
    N = lambda t: nt.nodes.new(t)
    L = nt.links.new
    def ramp(cols, lo=0.18, hi=0.55):
        r = N("ShaderNodeValToRGB"); cr = r.color_ramp; cr.interpolation = "EASE"      # soft shading (the owner: no hard edges)
        cr.elements[0].position, cr.elements[0].color = 0.0, lc.hex_linear(cols[0])
        cr.elements[1].position, cr.elements[1].color = lo, lc.hex_linear(cols[1])
        e = cr.elements.new(hi); e.color = lc.hex_linear(cols[2])
        return r
    def math(op, a, b=None, v=None):
        n = N("ShaderNodeMath"); n.operation = op
        if isinstance(a, float): n.inputs[0].default_value = a
        else: L(a, n.inputs[0])
        if b is not None:
            if isinstance(b, float): n.inputs[1].default_value = b
            else: L(b, n.inputs[1])
        return n.outputs[0]
    diff = N("ShaderNodeBsdfDiffuse"); diff.inputs["Color"].default_value = (1, 1, 1, 1)
    s2r = N("ShaderNodeShaderToRGB"); L(diff.outputs[0], s2r.inputs[0])
    rf, rw = ramp(fur), ramp(white)
    L(s2r.outputs["Color"], rf.inputs["Fac"]); L(s2r.outputs["Color"], rw.inputs["Fac"])
    tc = N("ShaderNodeTexCoord"); sep = N("ShaderNodeSeparateXYZ"); L(tc.outputs["Object"], sep.inputs[0])
    x, y, z = sep.outputs["X"], sep.outputs["Y"], sep.outputs["Z"]
    belly = math("MULTIPLY", math("LESS_THAN", z, math("ADD", math("MULTIPLY", x, 0.36), 0.165)), math("GREATER_THAN", z, 0.075))   # the underside, above the legs
    front_legs = math("MULTIPLY", math("GREATER_THAN", x, 0.02), math("LESS_THAN", z, 0.12))        # the front legs are white (L3)
    muzzle = math("MULTIPLY", math("MULTIPLY", math("GREATER_THAN", x, 0.212), math("LESS_THAN", z, 0.318)),
                  math("LESS_THAN", math("ABSOLUTE", y), 0.046))                                     # the short muzzle, white, only as wide as the muzzle
    blaze = math("MULTIPLY", math("MULTIPLY", math("LESS_THAN", math("ABSOLUTE", y), 0.011), math("GREATER_THAN", x, 0.17)),
                 math("GREATER_THAN", z, 0.30))                                                      # the blaze up the forehead
    paws = math("LESS_THAN", z, 0.028)
    mask = math("MAXIMUM", math("MAXIMUM", belly, muzzle), math("MAXIMUM", math("MAXIMUM", blaze, paws), front_legs))
    mix = N("ShaderNodeMixRGB"); mix.blend_type = "MIX"
    L(mask, mix.inputs["Fac"]); L(rf.outputs["Color"], mix.inputs["Color1"]); L(rw.outputs["Color"], mix.inputs["Color2"])
    emit = N("ShaderNodeEmission"); L(mix.outputs["Color"], emit.inputs["Color"])
    out = N("ShaderNodeOutputMaterial"); L(emit.outputs[0], out.inputs["Surface"])
    return m

def materials():
    fur, fixed = ramp_hex("fur")
    white, _ = ramp_hex("fur_w")
    return {
        "body":  body_material((fur[2], fur[4], fur[6]), (white[3], white[5], white[7])),
        "fawn":  lc.toon_material("lili_fawn", fur[2], fur[4], fur[6], soft=True),
        "white": lc.toon_material("lili_white", white[3], white[5], white[7], soft=True),
        "eye":   lc.toon_material("lili_eye", fixed["eye"], fixed["eye"], fixed["eye"]),
        "nose":  lc.toon_material("lili_nose", fixed["nose"], fixed["nose"], fixed["ear_pink"]),
        "pink":  lc.toon_material("lili_ear_pink", "#9a5f64", "#c9898c", "#e3aeb0", soft=True),           # the inner ear: the sheet's soft pink (the palette has none)
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

def ear(name, loc, rot, mat, inner, parent, coll="LILI", w=0.075, h=0.125, thick=0.014, inset=0.013, fold=0.0):
    """a big chihuahua ear (the sheet, shaped in the live Blender 2026-10-21): a rounded triangle, thick, with the
    pink inner ear inset on the front, subdivided soft; `fold` bends it (the drooping right ear, D-03).
    Local: X across, Z up, -Y the front."""
    import bmesh
    bm = bmesh.new(); pts = []; n = 14
    tip = 0.012                                                   # the tip is a small arc, not a point (soft)
    for i in range(n + 1):
        t = i / n; pts.append((tip + (w / 2 - tip) * (1 - t) ** 0.85 + 0.008 * math.sin(math.pi * t), 0, (h - tip) * t))
    for k in range(1, 6):                                         # the arc over the tip
        a = math.pi * k / 6; pts.append((tip * math.cos(a), 0, (h - tip) + tip * math.sin(a)))
    for i in range(1, n):
        t = 1 - i / n; pts.append((-(tip + (w / 2 - tip) * (1 - t) ** 0.85 + 0.010 * math.sin(math.pi * t)), 0, (h - tip) * t))
    verts = [bm.verts.new(p) for p in pts]
    face = bm.faces.new(verts)
    res = bmesh.ops.inset_region(bm, faces=[face], thickness=inset, depth=0.0, use_boundary=True, use_even_offset=True)
    ring = set(res["faces"])
    for f in bm.faces:
        f.material_index = 0 if f in ring else 1; f.smooth = True
        if f not in ring:
            for v in f.verts: v.co.y += 0.0025                     # the inner ear sits back a little (cupped)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    me.materials.append(mat); me.materials.append(inner)
    o = bpy.data.objects.new(name, me); lc.link(o, coll)
    o.parent = parent; o.location = loc; o.rotation_euler = rot; o.scale = (1.15, 1.0, 1.08)
    sol = o.modifiers.new("thick", "SOLIDIFY"); sol.thickness = thick; sol.offset = 1.0
    if fold:
        b = o.modifiers.new("fold", "SIMPLE_DEFORM"); b.deform_method = "BEND"; b.angle = math.radians(fold); b.deform_axis = "X"
    sub = o.modifiers.new("soft", "SUBSURF"); sub.levels = 2; sub.render_levels = 3
    return o

def tail(parent, mat, coll="LILI"):
    """the tail curled high over the back (L8): a bevelled Bezier curve, thinner towards the tip"""
    cu = bpy.data.curves.new("lili_tail_curve", "CURVE")
    cu.dimensions = "3D"; cu.bevel_depth = 0.021; cu.bevel_resolution = 6; cu.fill_mode = "FULL"; cu.use_fill_caps = True
    sp = cu.splines.new("BEZIER"); sp.bezier_points.add(3)
    pts = [((0.0, 0.0, 0.0), 1.0), ((-0.05, 0.0, 0.08), 0.9), ((-0.01, 0.015, 0.15), 0.7), ((0.07, 0.03, 0.13), 0.6)]
    for bp, (co, r) in zip(sp.bezier_points, pts):
        bp.co = co; bp.radius = r; bp.handle_left_type = bp.handle_right_type = "AUTO"
    o = bpy.data.objects.new("lili_tail_mesh", cu)
    o.data.materials.append(mat)
    o.parent = parent
    lc.link(o, coll)
    return o

def mball(name, parent, elements, mat=None, coll="LILI", base=False):
    """one metaball object of the "lili_mb" family: Blender blends "lili_mb", "lili_mb.001", "lili_mb.002"... (a
    NUMERIC suffix only: "lili_mb.head" would be its own family) into one smooth surface, whatever each is parented
    to; the base (no suffix) carries the resolution and the material. The visible surface sits at 0.575 x radius
    (field s (1 - (d/r)^2)^3 = threshold: 2 (1 - x^2)^3 = 0.6 -> x = 0.575), so radii here are 1.74 x the size wanted. elements: (type, co, radius, size, rot_y_deg)"""
    mb = bpy.data.metaballs.new(name)
    if base:
        mb.resolution = 0.010; mb.render_resolution = 0.003; mb.threshold = 0.6     # a fine surface: no facets on the render
    for typ, co, r, size, ry in elements:
        e = mb.elements.new(); e.type = typ; e.co = co; e.radius = r; e.stiffness = 2.0
        if size: e.size_x, e.size_y, e.size_z = size
        if ry: e.rotation = (math.cos(math.radians(ry) / 2), 0.0, math.sin(math.radians(ry) / 2), 0.0)   # about Y
    if mat: mb.materials.append(mat)
    o = bpy.data.objects.new(name, mb)
    o.parent = parent
    lc.link(o, coll)
    return o

def build(coll="LILI"):
    """one continuous fawn body (metaballs: body, neck, head, cheeks, muzzle, legs blend into a single
    surface, as on the approved sheet), the white markings, eyes, nose and ears as meshes on it"""
    M = materials()
    E = lambda name, loc, par: lc.link(_empty(name, loc, par), coll)
    root = E("lili_root", (0, 0, 0), None)
    head = E("lili_head", (0.105, 0, 0.292), root)                                             # the neck joint: the animation handle
    legs = [E(f"lili_leg{i}", (x, y, 0.115), root) for i, (x, y) in enumerate(((0.065, 0.050), (0.065, -0.050), (-0.072, 0.052), (-0.072, -0.052)))]
    t = E("lili_tail", (-0.125, 0, 0.212), root)
    # the fawn mass: the base object carries the family's resolution and material
    body = mball("lili_mb", root, [
        ("ELLIPSOID", (-0.005, 0, 0.172), 0.146, (1.08, 0.95, 0.85), 0),                    # the barrel, compact: an ELLIPSOID's size is a scale around its radius (1 = a ball)
        ("BALL", (-0.072, 0, 0.162), 0.105, None, 0),                                        # the rump
        ("BALL", (0.055, 0, 0.168), 0.117, None, 0),                                          # the chest mass
        ("CAPSULE", (0.08, 0, 0.225), 0.10, (0.02, 0, 0), -55),                              # the neck, short, up into the head
    ], M["body"], coll, base=True)
    mball("lili_mb.001", head, [                                                               # the head family member
        ("BALL", (0.035, 0, 0.03), 0.153, None, 0),                                           # the skull
        ("BALL", (0.045, 0, 0.075), 0.10, None, 0),                                          # the apple dome
        ("BALL", (0.06, 0.056, -0.022), 0.066, None, 0),                                      # puffy cheeks
        ("BALL", (0.06, -0.056, -0.022), 0.066, None, 0),
        ("BALL", (0.105, 0, -0.026), 0.060, None, 0),                                         # the short muzzle's root
    ], None, coll)
    for i, hip in enumerate(legs):
        mball(f"lili_mb.{i + 2:03d}", hip, [
            ("BALL", (0, 0, 0), 0.058, None, 0),                                              # the thigh / shoulder, blending into the barrel
            ("CAPSULE", (0, 0, -0.05), 0.036, (0.03, 0, 0), 90),                               # the leg, vertical
            ("ELLIPSOID", (0.012, 0, -0.098), 0.046, (1.25, 1.05, 0.62), 0),                   # the paw: wide, flat
            ("BALL", (0.036, -0.016, -0.102), 0.026, None, 0), ("BALL", (0.036, 0, -0.102), 0.026, None, 0), ("BALL", (0.036, 0.016, -0.102), 0.026, None, 0),   # three toes
        ], None, coll)
    # the white markings as meshes that stand a little proud of the fawn surface
    # the white markings (chest, belly, muzzle, blaze, paws) are painted by the body material's mask
    sphere("lili_nose", (0.152, 0, -0.008), (0.016, 0.018, 0.013), M["nose"], head)        # the worn pinkish-brown nose (L6)
    for side, y in (("l", 0.044), ("r", -0.044)):
        sphere(f"lili_eye_{side}", (0.096, y, 0.036), (0.028, 0.028, 0.030), M["eye"], head)   # big, round, dark, set apart (L5)
        sphere(f"lili_gloss_{side}", (0.118, y + (0.006 if side == "l" else -0.006), 0.047), (0.007, 0.007, 0.007), M["gloss"], head)
    ear("lili_ear_l", (0.028, 0.048, 0.122), (math.radians(-16), math.radians(6), math.radians(22)), M["fawn"], M["pink"], head)      # her left ear: up, a little outward, the pink forward
    ear("lili_ear_r", (0.028, -0.055, 0.112), (math.radians(140), math.radians(-4), math.radians(-28)), M["fawn"], M["pink"], head, fold=35)   # her RIGHT ear hangs beside the head, folded (D-03)
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
    # the size check: the standing dog must fit the 48 px cell with room for the head (spec 7.3: ~80 x 60 logical px at native 480)
    bpy.context.view_layer.update()
    xs, ys = [], []
    for o in lc.collection("LILI").objects:
        if o.type in ("MESH", "CURVE", "META"):
            for v in o.bound_box:
                p = lc.to_logical(sc, o.matrix_world @ Vector(v)); xs.append(p[0]); ys.append(p[1])
    print(f"lili: the model spans logical x {min(xs):.1f}..{max(xs):.1f}, y {min(ys):.1f}..{max(ys):.1f} in the cell (facing east; pivot at {lc.PIVOT})")
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
