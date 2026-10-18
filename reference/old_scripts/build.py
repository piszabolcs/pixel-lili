import bpy, math, sys
from mathutils import Vector
bpy.ops.wm.read_factory_settings(use_empty=True)
sc=bpy.context.scene
def mat(name,rgb):
    m=bpy.data.materials.new(name); m.use_nodes=True
    b=m.node_tree.nodes["Principled BSDF"]; b.inputs["Base Color"].default_value=(*rgb,1); b.inputs["Roughness"].default_value=1
    return m
def srgb(h):
    c=[int(h[i:i+2],16)/255 for i in (0,2,4)]
    return tuple(((x+0.055)/1.055)**2.4 if x>0.04045 else x/12.92 for x in c)
FAWN=mat("fawn",srgb("e2aa6e")); WHITE=mat("white",srgb("fff6e8")); PINK=mat("pink",srgb("eaa0a0"))
EYE=mat("eye",srgb("1c1618")); NOSE=mat("nose",srgb("d49a8c")); CREAM=mat("cream",srgb("f2d2a2"))
def obj(kind,loc,scale,m,parent=None,rot=(0,0,0)):
    if kind=="s": bpy.ops.mesh.primitive_uv_sphere_add(segments=16,ring_count=10,location=loc)
    elif kind=="c": bpy.ops.mesh.primitive_cylinder_add(vertices=10,location=loc)
    elif kind=="k": bpy.ops.mesh.primitive_cone_add(vertices=8,location=loc)
    o=bpy.context.object; o.scale=scale; o.rotation_euler=rot; o.data.materials.append(m)
    bpy.ops.object.shade_smooth()
    if parent: o.parent=parent; o.matrix_parent_inverse=parent.matrix_world.inverted()
    return o
root=bpy.data.objects.new("root",None); sc.collection.objects.link(root)
body=obj("s",(0,0,0.55),(0.55,0.32,0.30),FAWN,root)
obj("s",(0.12,0,0.45),(0.42,0.26,0.2),WHITE,root)          # belly/chest
neck=bpy.data.objects.new("neck",None); sc.collection.objects.link(neck); neck.location=(0.45,0,0.75); neck.parent=root
head=obj("s",(0.62,0,0.95),(0.30,0.30,0.27),CREAM,neck)
obj("s",(0.70,0,0.95),(0.22,0.06,0.25),WHITE,neck)          # blaze
obj("s",(0.84,0,0.86),(0.14,0.15,0.11),WHITE,neck)          # muzzle
obj("s",(0.97,0,0.89),(0.04,0.05,0.04),NOSE,neck)
for y in (0.13,-0.13):
    obj("s",(0.86,y*1.1,1.0),(0.07,0.075,0.08),EYE,neck)
earL=obj("k",(0.55,0.20,1.27),(0.11,0.06,0.20),FAWN,neck,rot=(math.radians(-20),0,0))
earR=obj("k",(0.55,-0.25,1.18),(0.11,0.06,0.20),FAWN,neck,rot=(math.radians(60),0,0))   # floppy ear
legs=[]
for x in (0.35,-0.35):
    for y in (0.16,-0.16):
        piv=bpy.data.objects.new("leg",None); sc.collection.objects.link(piv); piv.location=(x,y,0.5); piv.parent=root
        obj("c",(x,y,0.27),(0.065,0.065,0.25),WHITE if x>0 else FAWN,piv)
        obj("s",(x+0.03,y,0.04),(0.08,0.07,0.05),WHITE,piv)
        legs.append(piv)
tail=bpy.data.objects.new("tailp",None); sc.collection.objects.link(tail); tail.location=(-0.5,0,0.7); tail.parent=root
for i in range(5):
    a=i*0.55
    obj("s",(-0.5-0.12*math.sin(a)*1.2, 0, 0.7+0.12*(1-math.cos(a))*1.6),(0.07,0.07,0.07),FAWN,tail)
# ground shadow catcher
bpy.ops.mesh.primitive_plane_add(size=20); g=bpy.context.object; g.is_shadow_catcher=True
# light
bpy.ops.object.light_add(type='SUN',rotation=(math.radians(40),math.radians(15),math.radians(30))); bpy.context.object.data.energy=3.5; bpy.context.object.data.angle=0.2
w=bpy.data.worlds.new("w"); sc.world=w; w.use_nodes=True; w.node_tree.nodes["Background"].inputs[1].default_value=0.9
# camera: 3/4 top-down orthographic
bpy.ops.object.camera_add(location=(4.5,-4.5,4.2)); cam=bpy.context.object; sc.camera=cam
cam.data.type='ORTHO'; cam.data.ortho_scale=2.0
d=Vector((0,0,0.6))-cam.location; cam.rotation_euler=d.to_track_quat('-Z','Y').to_euler()
sc.render.engine='CYCLES'; sc.cycles.samples=24; sc.cycles.device='CPU'
sc.render.resolution_x=sc.render.resolution_y=64; sc.render.film_transparent=True
sc.render.filter_size=0.5; sc.view_settings.view_transform='Standard'
mode=sys.argv[-2]; DIR=int(sys.argv[-1]); N=8
for f in range(N):
    t=f/N*2*math.pi
    root.rotation_euler=(0,0,math.radians(DIR*45))
    if mode=='walk':
        for i,l in enumerate(legs):
            ph=t+(0 if i in (0,3) else math.pi); l.rotation_euler=(0,math.sin(ph)*0.5,0)
        root.location.z=abs(math.sin(t))*0.04
        tail.rotation_euler=(math.sin(t*2)*0.6,0,0)
        neck.rotation_euler=(0,math.sin(t)*0.05,0)
    if mode=='turn':
        neck.rotation_euler=(math.sin(t)*0.45,0,math.sin(t)*0.6)   # head tilt + look
    sc.render.filepath=f"/tmp/lili3d/r/{mode}{DIR}_{f}.png"; bpy.ops.render.render(write_still=True)
