"""live_helpers.py - the builders used while shaping Lili in the live Blender (MCP), 2026-10-21.
Imported there with importlib.reload after every edit. Everything works on the open lili.blend."""
import math
import bpy, bmesh
from mathutils import Vector, Matrix

def obj(n): return bpy.data.objects[n]
def coll(): return bpy.data.collections["LILI"]

def remove(*names):
    for n in names:
        o = bpy.data.objects.get(n)
        if o: bpy.data.objects.remove(o, do_unlink=True)

def ear(name, parent, loc, rot, mat, inner, w=0.075, h=0.125, thick=0.014, inset=0.013, fold=0.0, fold_at=0.5, fold_to="down"):
    """a chihuahua ear: rounded triangle, thick, pink inner inset on its front face, subdivided soft.
    rot: a 4x4 rotation Matrix (world-aligned; the ear's local -Y is its pink front, so Rotation(90, Z)
    turns the pink towards +X, the dog's front, and a Rotation about X before it tilts the ear sideways).
    fold: degrees the part above fold_at*h bends about the crease (the ear's local X); the sign is
    picked in WORLD space: fold_to "down" = the tip ends lowest, "in" = nearest the midline, "out" = farthest."""
    bm = bmesh.new(); pts = []; n = 14; tip = 0.012
    for i in range(n + 1):
        t = i / n; pts.append((tip + (w / 2 - tip) * (1 - t) ** 0.85 + 0.008 * math.sin(math.pi * t), 0, (h - tip) * t))
    for k in range(1, 6):
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
            for v in f.verts: v.co.y += 0.0025
    bmesh.ops.subdivide_edges(bm, edges=[e for e in bm.edges], cuts=3, use_grid_fill=True)
    rotm = rot.to_4x4() if hasattr(rot, "to_4x4") else rot
    Mw = parent.matrix_world @ Matrix.Translation(loc) @ rotm
    if fold:
        zc = fold_at * h
        best = None
        for sign in (1, -1):
            R = Matrix.Rotation(math.radians(fold) * sign, 4, "X")
            tipw = Mw @ (R @ (Vector((0, 0, h)) - Vector((0, 0, zc))) + Vector((0, 0, zc)))
            score = tipw.z if fold_to == "down" else (abs(tipw.y) if fold_to == "in" else -abs(tipw.y))
            if best is None or score < best[0]: best = (score, sign)
        sign = best[1]
        for v in bm.verts:
            if v.co.z > zc:
                k = min(1.0, (v.co.z - zc) / (0.22 * h))
                v.co = Matrix.Rotation(math.radians(fold) * sign * k, 4, "X") @ (v.co - Vector((0, 0, zc))) + Vector((0, 0, zc))
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    me.materials.append(mat); me.materials.append(inner)
    o = bpy.data.objects.new(name, me); coll().objects.link(o)
    o.parent = parent; o.location = loc; o.rotation_euler = rotm.to_euler("XYZ")
    sol = o.modifiers.new("thick", "SOLIDIFY"); sol.thickness = thick; sol.offset = 1.0
    sub = o.modifiers.new("soft", "SUBSURF"); sub.levels = 2; sub.render_levels = 3
    return o

def mb_family_member(name, parent, elements):
    """a lili_mb.NNN metaball object: elements (type, co, radius, size(x,y,z) or None, rot_y_deg)"""
    mb = bpy.data.metaballs.new(name)
    for typ, co, r, size, ry in elements:
        e = mb.elements.new(); e.type = typ; e.co = co; e.radius = r; e.stiffness = 2.0
        if size: e.size_x, e.size_y, e.size_z = size
        if ry: e.rotation = (math.cos(math.radians(ry) / 2), 0.0, math.sin(math.radians(ry) / 2), 0.0)
    o = bpy.data.objects.new(name, mb); coll().objects.link(o); o.parent = parent
    return o

def tail_chain(name, parent, path, r0=0.052, r1=0.030, n=12):
    """the tail as a chain of metaballs along a Catmull-Rom path (control points in the parent's space),
    radius tapering r0 -> r1: it blends into the rump and ends round"""
    def cr(p0, p1, p2, p3, t):
        t2, t3 = t * t, t * t * t
        return 0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3)
    P = [Vector(p) for p in path]
    P = [P[0]] + P + [P[-1]]
    els = []
    total = n
    for i in range(total + 1):
        u = i / total * (len(path) - 1)
        k = min(int(u), len(path) - 2); t = u - k
        p = cr(P[k], P[k + 1], P[k + 2], P[k + 3], t)
        r = r0 + (r1 - r0) * (i / total)
        els.append(("BALL", tuple(p), r, None, 0))
    return mb_family_member(name, parent, els)

def set_elements(name, elements):
    o = obj(name); mb = o.data
    for e in list(mb.elements): mb.elements.remove(e)
    for typ, co, r, size, ry in elements:
        e = mb.elements.new(); e.type = typ; e.co = co; e.radius = r; e.stiffness = 2.0
        if size: e.size_x, e.size_y, e.size_z = size
        if ry: e.rotation = (math.cos(math.radians(ry) / 2), 0.0, math.sin(math.radians(ry) / 2), 0.0)

def body_verts():
    dg = bpy.context.evaluated_depsgraph_get()
    ev = obj("lili_mb").evaluated_get(dg); m = ev.to_mesh(); n = len(m.vertices); ev.to_mesh_clear(); return n

def save(path):
    bpy.ops.wm.save_as_mainfile(filepath=path, compress=True)
