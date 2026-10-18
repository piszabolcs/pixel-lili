from PIL import Image, ImageDraw
W,H=40,40
FAWN=(226,170,110); FAWN_D=(196,138,82); CREAM=(244,214,166); WHITE=(255,248,236)
PINK=(236,160,160); NOSE=(196,128,116); NOSE_L=(226,170,160); EYE=(28,22,24); OUT=(70,44,30); TONG=(232,96,110)
def frame(wag=0, blink=False, pant=False):
    im=Image.new("RGBA",(W,H),(0,0,0,0)); d=ImageDraw.Draw(im)
    # tail (curled), wag shifts
    tx=29+wag
    d.ellipse([tx,19,tx+6,26],fill=FAWN); d.ellipse([tx+2,21,tx+4,24],fill=(0,0,0,0)); d.point((tx+5,19),fill=CREAM)
    # body
    d.ellipse([10,21,30,38],fill=FAWN); d.ellipse([12,26,28,38],fill=FAWN_D)
    d.ellipse([14,21,26,36],fill=WHITE)
    # legs
    for x in (14,23):
        d.rectangle([x,30,x+3,37],fill=WHITE); d.rectangle([x,37,x+3,38],fill=CREAM)
    # ears: left upright, right folded
    d.polygon([(9,13),(10,1),(17,8)],fill=FAWN); d.polygon([(11,10),(11,4),(15,8)],fill=PINK)
    d.polygon([(23,8),(31,3),(31,11)],fill=FAWN); d.polygon([(30,3),(35,6),(32,9)],fill=FAWN_D); d.polygon([(25,8),(29,6),(29,10)],fill=PINK)
    # head
    d.ellipse([8,6,32,25],fill=FAWN); d.ellipse([10,7,30,24],fill=CREAM)
    d.rectangle([19,6,21,15],fill=WHITE)  # blaze
    d.ellipse([13,15,27,24],fill=WHITE)   # muzzle
    # eyes (big, looking outward)
    if blink:
        d.line([12,13,16,13],fill=EYE); d.line([24,13,28,13],fill=EYE)
    else:
        d.ellipse([12,10,16,14],fill=EYE); d.ellipse([24,10,28,14],fill=EYE)
        d.point((12,11),fill=WHITE); d.point((28,11),fill=WHITE)
    # nose (worn light spot)
    d.ellipse([18,16,22,19],fill=NOSE); d.point((20,17),fill=NOSE_L); d.point((19,17),fill=NOSE_L)
    # mouth
    if pant:
        d.ellipse([17,20,23,24],fill=EYE); d.ellipse([18,22,22,26],fill=TONG)
    else:
        d.point((20,20),fill=OUT); d.point((20,21),fill=OUT)
        d.point((17,21),fill=OUT); d.point((18,22),fill=OUT); d.point((19,22),fill=OUT); d.point((21,22),fill=OUT); d.point((22,22),fill=OUT); d.point((23,21),fill=OUT)
    # outline
    px=im.load(); o=im.copy(); op=o.load()
    for y in range(H):
        for x in range(W):
            if px[x,y][3]==0:
                for dx,dy in((1,0),(-1,0),(0,1),(0,-1)):
                    X,Y=x+dx,y+dy
                    if 0<=X<W and 0<=Y<H and px[X,Y][3]>0: op[x,y]=OUT+(255,);break
    return o
frames=[frame(0),frame(1),frame(2),frame(1),frame(0),frame(1,pant=True),frame(2,pant=True),frame(1,pant=True),frame(0,blink=True),frame(0)]
S=10
big=[]
for f in frames:
    bg=Image.new("RGBA",(W*S,H*S),(250,236,214,255)); bg.alpha_composite(f.resize((W*S,H*S),Image.NEAREST)); big.append(bg.convert("P",palette=Image.ADAPTIVE))
big[0].save("lili_v1.gif",save_all=True,append_images=big[1:],duration=[120]*8+[160,120],loop=0)
sheet=Image.new("RGBA",(W*4,H),(0,0,0,0))
for i,f in enumerate([frame(0),frame(2),frame(1,pant=True),frame(0,blink=True)]): sheet.alpha_composite(f,(i*W,0))
sheet.save("lili_sheet_v1.png"); sheet.resize((W*4*8,H*8),Image.NEAREST).save("lili_preview_v1.png")
