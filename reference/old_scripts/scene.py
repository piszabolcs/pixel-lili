import math, sys
sys.path.insert(0,'/tmp/lili3d')
from PIL import Image, ImageDraw
exec(open('/tmp/lili3d/post.py').read().split("import sys")[0])
walk={d:[proc(f"/tmp/lili3d/r/walk{d}_{i}.png") for i in range(8)] for d in range(8)}
turn=[proc(f"/tmp/lili3d/r/turn7_{i}.png") for i in range(8)]
C=170; PPU=32; el=math.atan2(4.2-0.6, math.hypot(4.5,4.5))
def proj(x,y):
    sx=(x+y)/math.sqrt(2); sy=-(y-x)/math.sqrt(2)*math.sin(el)
    return C/2+sx*PPU, C/2+12+sy*PPU
# floor
floor=Image.new("RGBA",(C,C),(236,222,200,255)); d=ImageDraw.Draw(floor)
for i in range(-6,7):
    a=proj(i*0.5,-3);b=proj(i*0.5,3); d.line([a,b],fill=(222,206,182))
rug=[proj(-1.3,-1.1),proj(1.3,-1.1),proj(1.3,1.1),proj(-1.3,1.1)]
d.polygon(rug,fill=(170,40,40),outline=(110,24,24))
inner=[proj(-1.0,-0.8),proj(1.0,-0.8),proj(1.0,0.8),proj(-1.0,0.8)]
d.polygon(inner,outline=(236,210,170))
# toy (colourful knotted rope)
tx,ty=proj(-0.9,1.3)
for k,c in enumerate([(230,60,60),(60,120,220),(250,200,40),(60,180,90)]):
    d.ellipse([tx-6+k*3,ty-2,tx-2+k*3,ty+2],fill=c,outline=(60,40,30))
path=[(-1.6,2.0),(1.2,1.6),(1.6,-1.2),(-0.2,-1.6),(-0.6,0.4)]
frames=[];durs=[]
pos=list(path[0]); step=0.09; fi=0
for tgt in path[1:]:
    while True:
        dx,dy=tgt[0]-pos[0],tgt[1]-pos[1]; dist=math.hypot(dx,dy)
        if dist<step: break
        pos[0]+=dx/dist*step; pos[1]+=dy/dist*step
        dirn=round(math.degrees(math.atan2(dy,dx))/45)%8
        sp=walk[dirn][fi%8]; fi+=1
        f=floor.copy(); px,py=proj(*pos); f.alpha_composite(sp,(int(px-32),int(py-40)))
        frames.append(f); durs.append(70)
px,py=proj(*pos)
for k in range(2):
    for sp in turn:
        f=floor.copy(); f.alpha_composite(sp,(int(px-32),int(py-40))); frames.append(f); durs.append(110)
big=[f.resize((C*3,C*3),Image.NEAREST).convert("P",palette=Image.ADAPTIVE) for f in frames]
big[0].save("/home/claude/lili_3d_proba.gif",save_all=True,append_images=big[1:],duration=durs,loop=0)
print(len(frames))
