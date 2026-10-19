#!/usr/bin/env python3
from PIL import Image, ImageDraw, ImageFilter
from pathlib import Path
import math

ROOT = Path(__file__).resolve().parent / 'assets' / 'ford_ipc'
ROOT.mkdir(parents=True, exist_ok=True)

OUT=360
SS=4
S=OUT*SS
CX=CY=S//2

def sc(v): return int(round(v*SS))

def rgba(c): return c

def angle_point(deg, radius):
    a=math.radians(deg)
    return (CX+math.cos(a)*sc(radius), CY+math.sin(a)*sc(radius))

def radial_gradient(size, inner, outer):
    im=Image.new('RGBA',(size,size),(0,0,0,0))
    p=im.load(); c=size/2; maxr=size/2
    for y in range(size):
        for x in range(size):
            r=min(1.0, math.hypot(x-c,y-c)/maxr)
            # slightly brighter upper left, darker lower right
            light=max(0.0, 1.0-math.hypot(x-c*0.72,y-c*0.62)/(maxr*1.15))
            t=min(1,max(0,r))
            vals=[int(inner[i]*(1-t)+outer[i]*t + (10 if i<3 else 0)*light) for i in range(3)]
            a=int(inner[3]*(1-t)+outer[3]*t)
            p[x,y]=(max(0,min(255,vals[0])),max(0,min(255,vals[1])),max(0,min(255,vals[2])),a)
    return im

def arc(draw, bbox, start, end, fill, width):
    draw.arc(tuple(sc(v) for v in bbox), start=start, end=end, fill=fill, width=sc(width))

def make_face(tach=False):
    # transparent canvas
    im=Image.new('RGBA',(S,S),(0,0,0,0))
    # soft outer shadow
    sh=Image.new('RGBA',(S,S),(0,0,0,0)); sd=ImageDraw.Draw(sh)
    sd.ellipse((sc(11),sc(13),S-sc(11),S-sc(9)), fill=(0,0,0,165))
    sh=sh.filter(ImageFilter.GaussianBlur(sc(7)))
    im.alpha_composite(sh)

    # metallic bezel gradients by concentric circles
    d=ImageDraw.Draw(im,'RGBA')
    d.ellipse((sc(5),sc(5),S-sc(5),S-sc(5)), fill=(4,11,16,245), outline=(88,145,170,210), width=sc(2))
    d.ellipse((sc(10),sc(10),S-sc(10),S-sc(10)), fill=(12,27,38,255), outline=(132,185,205,220), width=sc(2))
    d.ellipse((sc(17),sc(17),S-sc(17),S-sc(17)), fill=(7,17,25,255), outline=(34,83,105,235), width=sc(3))

    # face radial shading
    face = radial_gradient(S, (13,29,42,255), (1,6,10,255))
    mask=Image.new('L',(S,S),0); md=ImageDraw.Draw(mask)
    md.ellipse((sc(22),sc(22),S-sc(22),S-sc(22)), fill=255)
    face.putalpha(mask)
    im.alpha_composite(face)
    d=ImageDraw.Draw(im,'RGBA')

    # inner shadow/ring
    d.ellipse((sc(25),sc(25),S-sc(25),S-sc(25)), outline=(0,0,0,150), width=sc(5))
    d.ellipse((sc(31),sc(31),S-sc(31),S-sc(31)), outline=(57,112,137,125), width=sc(1))

    # subtle inner arc highlights
    arc(d,(34,34,OUT-34,OUT-34),205,330,(105,188,215,70),1)
    arc(d,(34,34,OUT-34,OUT-34),20,140,(5,15,20,180),2)

    START=140.0; SWEEP=260.0
    maxv=8000 if tach else 160
    major_step=1000 if tach else 20
    majors=maxv//major_step
    minor_div=5
    total=int(majors*minor_div)
    outer=158
    for j in range(total+1):
        v=maxv*j/total
        deg=START+SWEEP*(v/maxv)
        major=(j%minor_div)==0
        red=tach and v>=6000
        r1=134 if major else 145
        r2=159
        a=angle_point(deg,r1); b=angle_point(deg,r2)
        col=(242,247,249,245) if major else (99,126,139,210)
        if red: col=(238,57,63,255) if major else (191,42,48,235)
        d.line((a,b),fill=col,width=sc(4 if major else 2))
        if major:
            # add a tiny beveled inner highlight
            a2=angle_point(deg,r1+2); b2=angle_point(deg,r2-1)
            hi=(255,255,255,90) if not red else (255,138,140,95)
            d.line((a2,b2),fill=hi,width=sc(1))

    if tach:
        # redline band from 6000 through 8000
        rs=START+SWEEP*(6000/maxv)
        bbox=(sc(18),sc(18),S-sc(18),S-sc(18))
        d.arc(bbox,start=rs,end=START+SWEEP,fill=(105,13,22,220),width=sc(7))
        bbox2=(sc(21),sc(21),S-sc(21),S-sc(21))
        d.arc(bbox2,start=rs,end=START+SWEEP,fill=(235,47,55,245),width=sc(2))

    # lower aperture shading to give analog bowl depth while leaving digital readout readable
    lower=Image.new('RGBA',(S,S),(0,0,0,0)); ld=ImageDraw.Draw(lower,'RGBA')
    ld.ellipse((sc(55),sc(196),S-sc(55),S-sc(5)), fill=(0,0,0,52))
    lower=lower.filter(ImageFilter.GaussianBlur(sc(18)))
    im.alpha_composite(lower)

    # glass reflection
    gl=Image.new('RGBA',(S,S),(0,0,0,0)); gd=ImageDraw.Draw(gl,'RGBA')
    gd.ellipse((sc(42),sc(28),S-sc(42),sc(146)),fill=(166,222,242,18))
    gd.arc((sc(33),sc(30),S-sc(33),S-sc(40)),start=205,end=300,fill=(208,241,250,50),width=sc(3))
    gl=gl.filter(ImageFilter.GaussianBlur(sc(3)))
    im.alpha_composite(gl)

    return im.resize((OUT,OUT),Image.Resampling.LANCZOS)

def make_needle(color=(135,208,233,255), highlight=(226,250,255,255)):
    im=Image.new('RGBA',(S,S),(0,0,0,0))
    d=ImageDraw.Draw(im,'RGBA')
    # broad counterweight and tapered needle, oriented to the right
    # subtle dark border first
    poly=[(sc(144),CY-sc(7)),(sc(185),CY-sc(9)),(sc(327),CY-sc(2)),(sc(340),CY),(sc(327),CY+sc(2)),(sc(185),CY+sc(9)),(sc(144),CY+sc(7))]
    d.polygon(poly,fill=(2,8,11,245))
    inner=[(sc(147),CY-sc(5)),(sc(186),CY-sc(7)),(sc(326),CY-sc(1)),(sc(337),CY),(sc(326),CY+sc(1)),(sc(186),CY+sc(7)),(sc(147),CY+sc(5))]
    d.polygon(inner,fill=color)
    # top-side specular highlight
    d.line((sc(160),CY-sc(3),sc(326),CY-sc(1)),fill=highlight,width=sc(1))
    # counterweight
    d.rounded_rectangle((sc(129),CY-sc(10),sc(177),CY+sc(10)),radius=sc(7),fill=(31,79,99,255),outline=(154,219,239,190),width=sc(1))
    return im.resize((OUT,OUT),Image.Resampling.LANCZOS)

def make_shadow():
    im=Image.new('RGBA',(S,S),(0,0,0,0)); d=ImageDraw.Draw(im,'RGBA')
    d.polygon([(sc(140),CY-sc(10)),(sc(185),CY-sc(11)),(sc(340),CY),(sc(185),CY+sc(11)),(sc(140),CY+sc(10))],fill=(0,0,0,175))
    d.rounded_rectangle((sc(127),CY-sc(13),sc(178),CY+sc(13)),radius=sc(8),fill=(0,0,0,150))
    im=im.filter(ImageFilter.GaussianBlur(sc(4)))
    return im.resize((OUT,OUT),Image.Resampling.LANCZOS)

def make_hub():
    im=Image.new('RGBA',(S,S),(0,0,0,0))
    d=ImageDraw.Draw(im,'RGBA')
    # shadow under hub
    d.ellipse((CX-sc(27)+sc(3),CY-sc(27)+sc(5),CX+sc(27)+sc(3),CY+sc(27)+sc(5)),fill=(0,0,0,130))
    # metallic rings
    d.ellipse((CX-sc(26),CY-sc(26),CX+sc(26),CY+sc(26)),fill=(7,17,23,255),outline=(127,185,206,245),width=sc(3))
    d.ellipse((CX-sc(20),CY-sc(20),CX+sc(20),CY+sc(20)),fill=(23,62,79,255),outline=(53,116,141,255),width=sc(2))
    d.ellipse((CX-sc(13),CY-sc(13),CX+sc(13),CY+sc(13)),fill=(102,181,210,255),outline=(206,239,248,235),width=sc(1))
    d.ellipse((CX-sc(6),CY-sc(6),CX+sc(6),CY+sc(6)),fill=(24,60,75,255))
    d.ellipse((CX-sc(10),CY-sc(10),CX-sc(3),CY-sc(3)),fill=(236,251,255,100))
    return im.resize((OUT,OUT),Image.Resampling.LANCZOS)

make_face(False).save(ROOT/'gauge_speed_face.png')
make_face(True).save(ROOT/'gauge_tach_face.png')
make_shadow().save(ROOT/'gauge_needle_shadow.png')
make_needle().save(ROOT/'gauge_needle.png')
make_hub().save(ROOT/'gauge_hub.png')
print('generated gauge assets in',ROOT)
