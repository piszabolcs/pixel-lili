"""lili_common.py - the one camera, light and material language of every Pixel Lili asset
(spec 7.1, 7.3, 7.5). Imported by the headless Blender scripts in this folder
(scene_setup.py now; render_all.py, the room and the yard later), never run alone.

The numbers that make the background and Lili fit together live here and nowhere else:
    ELEVATION_DEG  the camera looks down 35 degrees, orthographic, facing +Y (north):
                   the back wall is a horizontal line on the screen, as in room.json
    PX_M           one logical pixel = 1.25 cm at the ground; a ground depth d shows as
                   d * sin(35) on the screen, a height h as h * cos(35)
    CELL / RS      a sprite cell is 48 x 48 logical px, rendered 4x (192 x 192) for the pixelizer
    PIVOT          the paws' centre on the ground lands on logical (24, 40) of the cell
The light comes from the upper left of the picture (spec 7.1): from the left, from behind
and above, so the tops and left sides catch it, as in hand-drawn pixel art.
"""
import math
import bpy
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view

ELEVATION_DEG = 35.0
PX_M = 0.0125
CELL = 48
RS = 4
PIVOT = (24, 40)
LOG = 240                                   # the world's logical grid (common/lili/config.h)
SUN_AZIMUTH_DEG = 135.0                     # where the light comes FROM, seen from above: 0 = +X (east), 90 = +Y (north/back)
SUN_ELEVATION_DEG = 50.0
DIR_NAMES = ("S", "SW", "W", "NW", "N", "NE", "E", "SE")   # common/lili/lili_types.h dir8_t: clockwise from south (towards the viewer)

def heading_deg(d):
    """dir8_t -> the model's turn about Z: the model faces +X (east) at 0, S is towards the camera (-Y)"""
    return -90.0 - 45.0 * d

COLLECTIONS = ("CAMERA_LIGHT", "LILI", "LILI_HELPERS", "ROOM_BG", "ROOM_OCCLUDERS", "YARD_BG", "YARD_OCCLUDERS", "PROPS", "CHECK")

def srgb_to_linear(c):
    return ((c + 0.055) / 1.055) ** 2.4 if c > 0.04045 else c / 12.92

def hex_linear(h):
    """'#e2aa6e' -> linear RGBA, the way Blender stores a colour that should come out as that sRGB value"""
    h = h.lstrip("#")
    return tuple(srgb_to_linear(int(h[i:i + 2], 16) / 255.0) for i in (0, 2, 4)) + (1.0,)

def collection(name):
    c = bpy.data.collections.get(name)
    if c is None:
        c = bpy.data.collections.new(name)
        bpy.context.scene.collection.children.link(c)
    return c

def link(obj, coll_name):
    for c in obj.users_collection:
        c.objects.unlink(obj)
    collection(coll_name).objects.link(obj)
    return obj

def render_settings(scene, w, h):
    """EEVEE (Shader to RGB, the toon ramp, needs it), exact colours: Standard view, no dither, RGBA PNG"""
    for eng in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
        try:
            scene.render.engine = eng
            break
        except TypeError:
            continue
    scene.eevee.taa_render_samples = 16      # clean alpha edges: the pixelizer's 50 % coverage rule reads them
    scene.render.resolution_x, scene.render.resolution_y = w, h
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = True
    scene.render.filter_size = 0.75
    scene.render.dither_intensity = 0.0      # a palette colour must come out as itself
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
    scene.view_settings.exposure = 0.0
    scene.view_settings.gamma = 1.0
    im = scene.render.image_settings
    im.file_format = "PNG"
    im.color_mode = "RGBA"
    im.color_depth = "8"

def camera(scene, kind="cell"):
    """the one camera. kind 'cell': a 48 x 48 sprite cell with the pivot at the world origin;
    'room': the whole 240 x 240 view, logical (0, 0) at the top-left, the world origin at logical (0, 240)"""
    cam = scene.camera
    if cam is None:
        cam = bpy.data.objects.new("Camera", bpy.data.cameras.new("Camera"))
        link(cam, "CAMERA_LIGHT")
        scene.camera = cam
    cd = cam.data
    cd.type = "ORTHO"
    cd.sensor_fit = "AUTO"
    cd.clip_start, cd.clip_end = 0.1, 100.0
    cam.rotation_euler = (math.radians(90.0 - ELEVATION_DEG), 0.0, 0.0)   # from looking down (-Z) up to 35 degrees below the horizon, facing +Y
    back = Vector((0.0, -math.cos(math.radians(ELEVATION_DEG)), math.sin(math.radians(ELEVATION_DEG)))) * 20.0
    if kind == "cell":
        cd.ortho_scale = CELL * PX_M
        cd.shift_x = 0.0
        cd.shift_y = (PIVOT[1] - CELL / 2) / CELL          # the origin a third below the centre: room for the head above
        cam.location = back
        render_settings(scene, CELL * RS, CELL * RS)
    else:
        cd.ortho_scale = LOG * PX_M
        cd.shift_x, cd.shift_y = 0.5, 0.5                   # the world origin at the picture's bottom-left corner
        cam.location = back
        render_settings(scene, LOG * RS, LOG * RS)
    bpy.context.view_layer.update()                         # matrix_world follows location/rotation only after this: to_logical reads it
    return cam

def to_logical(scene, p):
    """a world point -> logical px of the current camera's frame (x right, y down), as the game counts"""
    v = world_to_camera_view(scene, scene.camera, Vector(p))
    w = scene.render.resolution_x / RS
    h = scene.render.resolution_y / RS
    return (v.x * w, (1.0 - v.y) * h)

def sun():
    o = bpy.data.objects.get("Sun")
    if o is None:
        o = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
        link(o, "CAMERA_LIGHT")
    o.data.energy = 3.0
    o.data.angle = math.radians(2.0)                       # nearly sharp: toon bands want crisp terminators
    az, el = math.radians(SUN_AZIMUTH_DEG), math.radians(SUN_ELEVATION_DEG)
    towards = -Vector((math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el)))
    o.rotation_euler = towards.to_track_quat("-Z", "Y").to_euler()
    return o

def world_black(scene):
    w = scene.world or bpy.data.worlds.new("World")
    scene.world = w
    if hasattr(w, "use_nodes"):
        try:
            w.use_nodes = True
        except AttributeError:
            pass
    bg = w.node_tree.nodes.get("Background")
    if bg:
        bg.inputs[0].default_value = (0, 0, 0, 1)
        bg.inputs[1].default_value = 0.0                    # no ambient: the ramp alone decides the tones

def toon_material(name, shadow, mid, light, lo=0.12, hi=0.45):
    """the 3-band toon of spec 7.1 / B4: the lit diffuse value -> a constant ramp of three palette
    colours (hex sRGB) -> emission, so the rendered pixel IS the palette colour (no lighting on top)"""
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    if hasattr(m, "use_nodes"):
        try:
            m.use_nodes = True
        except AttributeError:
            pass
    nt = m.node_tree
    nt.nodes.clear()
    diff = nt.nodes.new("ShaderNodeBsdfDiffuse")
    diff.inputs["Color"].default_value = (1, 1, 1, 1)
    s2r = nt.nodes.new("ShaderNodeShaderToRGB")
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    cr = ramp.color_ramp
    cr.interpolation = "CONSTANT"
    cr.elements[0].position, cr.elements[0].color = 0.0, hex_linear(shadow)
    cr.elements[1].position, cr.elements[1].color = lo, hex_linear(mid)
    e = cr.elements.new(hi)
    e.color = hex_linear(light)
    emit = nt.nodes.new("ShaderNodeEmission")
    emit.inputs["Strength"].default_value = 1.0
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(diff.outputs[0], s2r.inputs[0])
    nt.links.new(s2r.outputs["Color"], ramp.inputs["Fac"])
    nt.links.new(ramp.outputs["Color"], emit.inputs["Color"])
    nt.links.new(emit.outputs[0], out.inputs["Surface"])
    return m
