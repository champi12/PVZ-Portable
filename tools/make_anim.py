#!/usr/bin/env python3
"""Convierte reanim.json (salida de parse_reanim.py) a data/anim.pak para la PSP.
Por archivo: 'RNM1', u16 ntracks, u16 nframes, u16 fps, u16 pad, f32 bbox[4] (frame inicial),
  luego ntracks*nframes frames de 28 bytes: f32 x,y,sx,sy,kx,ky (rad), s16 img, s8 vis, u8 pad.
Cabecera del pak: 'ANIM', u32 n, u32 offs[n+1]."""
import json, struct, sys, math
from PIL import Image
R=json.load(open(sys.argv[1])); imgdir=sys.argv[2]; out=sys.argv[3]
sizes={}
def isz(i):
    if i not in sizes: sizes[i]=Image.open(f'{imgdir}/{i:03d}.png').size
    return sizes[i]
blobs=[]
for n,r in enumerate(R):
    tr=r['tracks']; nf=max(len(t) for t in tr)
    # bbox del primer frame visible
    xs=[];ys=[]
    f0=0
    for f in range(nf):
        if any(f<len(t) and t[f][0]!=-1 and t[f][7]>=0 for t in tr): f0=f; break
    for t in tr:
        if f0>=len(t): continue
        g,x,y,sx,sy,kx,ky,im=t[f0]
        if g==-1 or im<0: continue
        sx=(sx or 4096)/4096; sy=(sy or 4096)/4096
        ax=-kx*math.pi/180/4096; ay=-ky*math.pi/180/4096
        a=math.cos(ax)*sx; b=-math.sin(ax)*sx; c=math.sin(ay)*sy; d=math.cos(ay)*sy
        W,H=isz(im)
        for u,v in ((0,0),(W,0),(0,H),(W,H)):
            xs.append(x/4096+a*u+c*v); ys.append(y/4096+b*u+d*v)
    bbox=(min(xs),min(ys),max(xs),max(ys)) if xs else (0,0,0,0)
    b=bytearray(b'RNM1'+struct.pack('<HHHH',len(tr),nf,r['fps'],0)+struct.pack('<4f',*bbox))
    for t in tr:
        for f in range(nf):
            g,x,y,sx,sy,kx,ky,im=t[min(f,len(t)-1)]
            b+=struct.pack('<6fhbB',x/4096,y/4096,(sx or 4096)/4096,(sy or 4096)/4096,
                           -kx*math.pi/180/4096,-ky*math.pi/180/4096,im,1 if g!=-1 else 0,0)
    blobs.append(bytes(b))
off=8+4*(len(blobs)+1); offs=[]
for b in blobs: offs.append(off); off+=len(b)
offs.append(off)
with open(out,'wb') as f:
    f.write(b'ANIM'+struct.pack('<I',len(blobs))+struct.pack('<%dI'%len(offs),*offs))
    for b in blobs: f.write(b)
print(out,off,'bytes')
