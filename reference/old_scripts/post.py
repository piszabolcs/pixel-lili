from PIL import Image
OUT=(70,44,30,255)
def proc(p):
    im=Image.open(p).convert("RGBA"); px=im.load(); W,H=im.size
    for y in range(H):
        for x in range(W):
            r,g,b,a=px[x,y]; px[x,y]=(r,g,b,255) if a>140 else ((0,0,0,90) if a>25 else (0,0,0,0))
    rgb=im.convert("RGB").quantize(colors=14,method=Image.Quantize.MEDIANCUT).convert("RGBA")
    q=rgb.load()
    for y in range(H):
        for x in range(W):
            if px[x,y][3]!=255: q[x,y]=px[x,y]
    o=rgb.copy(); op=o.load()
    for y in range(H):
        for x in range(W):
            if q[x,y][3]<255:
                for dx,dy in((1,0),(-1,0),(0,1),(0,-1)):
                    X,Y=x+dx,y+dy
                    if 0<=X<W and 0<=Y<H and q[X,Y][3]==255: op[x,y]=OUT;break
    return o
import sys
for m in ("walk","turn","dirs"):
    fr=[proc(f"/tmp/lili3d/{m}_{i}.png") for i in range(8)]
    sheet=Image.new("RGBA",(64*8,64),(0,0,0,0))
    for i,f in enumerate(fr): sheet.alpha_composite(f,(i*64,0))
    bg=Image.new("RGBA",sheet.size,(244,232,214,255)); bg.alpha_composite(sheet)
    bg.resize((64*8*3,64*3),Image.NEAREST).save(f"/tmp/lili3d/sheet_{m}.png")
    for i,f in enumerate(fr): f.save(f"/tmp/lili3d/p_{m}_{i}.png")
