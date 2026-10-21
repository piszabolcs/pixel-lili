"""lili_rig_placeholder.py - the stand-in chihuahua until M3.4's model (spec 7.7: nothing waits
for the art). Primitives with the toon materials, parented to a root, with the handles every
animation in render_all.py moves: root (position, heading), head (nod, tilt), legs[4]
(swing about Y), tail (wag), ears[2]. The same handle names will exist on the real rig,
so render_all.py does not change when the model arrives.

Her RIGHT ear droops (D-03). The model faces +X at heading 0 (lili_common.heading_deg)."""
import math
import bpy
import lili_common as lc

def prim(kind, loc, scale, mat, parent, rot=(0, 0, 0), coll="LILI"):
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
    lc.link(o, coll)
    return o

def empty(name, loc, parent, coll="LILI"):
    e = bpy.data.objects.new(name, None)
    e.location = loc
    e.parent = parent
    lc.link(e, coll)
    return e

def build(coll="LILI"):
    """-> dict of handles; colours from the master palette's bases (assets/palettes/master.json via the caller, or these)"""
    FAWN = lc.toon_material("fawn", "#b47a56", "#f3b786", "#fcd5aa")
    WHITE = lc.toon_material("white", "#c9b9ad", "#fdedda", "#fffaf2")
    DARK = lc.toon_material("dark", "#120c0c", "#27101f", "#3a3034")
    NOSE = lc.toon_material("nose", "#4a2a30", "#77484c", "#9a6a6c")
    root = empty("lili_root", (0, 0, 0), None, coll)
    body = prim("s", (0.00, 0, 0.17), (0.16, 0.09, 0.085), FAWN, root, coll=coll)
    prim("s", (0.05, 0, 0.14), (0.11, 0.07, 0.06), WHITE, root, coll=coll)                       # chest and belly
    head = empty("lili_head", (0.14, 0, 0.26), root, coll)                                      # the neck joint
    prim("s", (0.03, 0, 0.01), (0.075, 0.075, 0.07), FAWN, head, coll=coll)                      # the apple head
    prim("s", (0.09, 0, -0.015), (0.04, 0.04, 0.03), WHITE, head, coll=coll)                     # muzzle
    prim("s", (0.126, 0, -0.008), (0.011, 0.013, 0.01), NOSE, head, coll=coll)
    for y in (0.03, -0.03):
        prim("s", (0.085, y, 0.025), (0.012, 0.012, 0.014), DARK, head, coll=coll)               # eyes
    ear_l = prim("k", (0.01, 0.05, 0.085), (0.032, 0.014, 0.04), FAWN, head, rot=(math.radians(-20), 0, 0), coll=coll)   # her left ear: up
    ear_r = prim("k", (0.01, -0.075, 0.05), (0.032, 0.014, 0.04), FAWN, head, rot=(math.radians(75), 0, 0), coll=coll)   # her RIGHT ear droops (D-03)
    legs = []
    for i, (x, y) in enumerate(((0.09, 0.045), (0.09, -0.045), (-0.09, 0.045), (-0.09, -0.045))):
        hip = empty(f"lili_leg{i}", (x, y, 0.12), root, coll)                                    # the hip / shoulder joint
        prim("c", (0, 0, -0.06), (0.018, 0.018, 0.06), WHITE if x > 0 else FAWN, hip, coll=coll)
        prim("s", (0.01, 0, -0.115), (0.024, 0.02, 0.014), WHITE, hip, coll=coll)                # paw
        legs.append(hip)
    tail = empty("lili_tail", (-0.15, 0, 0.22), root, coll)
    prim("k", (-0.02, 0, 0.03), (0.018, 0.018, 0.07), FAWN, tail, rot=(0, math.radians(-35), 0), coll=coll)
    return {"root": root, "body": body, "head": head, "legs": legs, "tail": tail, "ears": [ear_l, ear_r]}

def pose_rest(h):
    h["root"].location = (0, 0, 0)
    h["head"].rotation_euler = (0, 0, 0)
    h["tail"].rotation_euler = (0, 0, 0)
    for l in h["legs"]:
        l.rotation_euler = (0, 0, 0)

# ---- the placeholder animations: name -> f(handles, phase 0..1) (spec 7.4 names) ----
def anim_idle_stand(h, t):
    pose_rest(h)
    p = 2 * math.pi * t
    h["root"].location = (0, 0, 0.003 * math.sin(p))                           # breathing
    h["head"].rotation_euler = (0, math.radians(2.0 * math.sin(p)), 0)         # a slight nod
    h["tail"].rotation_euler = (math.radians(12 * math.sin(2 * p)), 0, 0)      # a lazy wag

def anim_walk(h, t):
    pose_rest(h)
    p = 2 * math.pi * t
    for i, l in enumerate(h["legs"]):
        ph = p + (0 if i in (0, 3) else math.pi)                               # diagonal pairs together
        l.rotation_euler = (0, math.radians(25 * math.sin(ph)), 0)
    h["root"].location = (0, 0, 0.006 * abs(math.sin(p)))
    h["head"].rotation_euler = (0, math.radians(3 * math.sin(p)), 0)
    h["tail"].rotation_euler = (math.radians(20 * math.sin(2 * p)), 0, 0)

ANIMS = {"idle_stand": anim_idle_stand, "walk": anim_walk}
