#!/usr/bin/env python3
"""Empaqueta las 605 imagenes extraidas del J2ME en data/gfx.pak listo para el GU de PSP.
- <=256 colores  -> T8 + CLUT 8888 (1 byte/pixel, ideal para PSP-1000)
- opacas grandes -> 5650 (2 bytes/pixel)
- resto          -> 8888
Todas swizzled (mas rapidas en GU), filas alineadas a 16 bytes, alto multiplo de 8.
Imagenes de mas de 512 px se dividen en tiles de <=512."""
import sys, os, json, struct
import numpy as np
from PIL import Image
FMT_T8, FMT_565, FMT_8888 = 0, 1, 2
def log2ceil(n):
    p=0
    while (1<<p)<n: p+=1
    return p
def swizzle(buf, wbytes, rows):
    a=np.frombuffer(buf,dtype=np.uint8).reshape(rows//8,8,wbytes//16,16)
    return a.transpose(0,2,1,3).tobytes()
def pack_tile(arr, fmt, bpp):
    h,w=arr.shape[:2]
    tbw=w
    while (tbw*bpp)%16: tbw+=1
    rows=(h+7)//8*8
    if arr.ndim==2: pad=np.zeros((rows,tbw),dtype=arr.dtype)
    else: pad=np.zeros((rows,tbw)+arr.shape[2:],dtype=arr.dtype)
    pad[:h,:w]=arr
    raw=pad.tobytes()
    return swizzle(raw,tbw*bpp,rows), tbw, rows
def convert(path, flip):
    im=Image.open(path).convert('RGBA'); a=np.array(im); h,w=a.shape[:2]
    a=a.copy(); a[a[:,:,3]==0]=0          # todo lo transparente = (0,0,0,0)
    flat=a.reshape(-1,4)
    uniq,inv=np.unique(flat,axis=0,return_inverse=True)
    inv=inv.reshape(-1)
    clut=b''
    # el indice 0 de la CLUT debe ser transparente: el relleno de la textura usa 0 y el
    # filtro bilineal lo muestrea en los bordes (si no, sale un "recuadro" alrededor)
    if not (uniq[0]==0).all():
        uniq=np.vstack([np.zeros((1,4),uniq.dtype),uniq]); inv=inv+1
    if len(uniq)<=256:
        fmt=FMT_T8; bpp=1
        clut=np.zeros((256,4),np.uint8); clut[:len(uniq)]=uniq; clut=clut.tobytes()
        px=inv.reshape(h,w).astype(np.uint8)
    elif (a[:,:,3]==255).all():
        fmt=FMT_565; bpp=2
        r=a[:,:,0].astype(np.uint16)>>3; g=a[:,:,1].astype(np.uint16)>>2; b=a[:,:,2].astype(np.uint16)>>3
        px=(r|(g<<5)|(b<<11)).astype('<u2')
    else:
        fmt=FMT_8888; bpp=4
        px=a.view('<u4').reshape(h,w)
    tiles=[]
    for ty in range(0,h,512):
        for tx in range(0,w,512):
            sub=px[ty:ty+512,tx:tx+512]; th_,tw_=sub.shape[:2]
            data,tbw,rows=pack_tile(sub,fmt,bpp)
            tiles.append((tx,ty,tw_,th_,tbw,rows,log2ceil(tw_),log2ceil(th_),data))
    return w,h,fmt,flip,clut,tiles
def main(imgdir,out):
    meta=json.load(open(os.path.join(imgdir,'meta.json')))
    n=len(meta); blobs=[]; entries=[]
    off=12+n*16
    for m in meta:
        w,h,fmt,flip,clut,tiles=convert(os.path.join(imgdir,'%03d.png'%m['id']),m['flip'])
        body=bytearray()
        body+=clut
        hdr_size=len(tiles)*20
        dpos=len(body)+hdr_size
        dpos=(dpos+15)&~15
        hdrs=bytearray(); datas=bytearray()
        for (tx,ty,tw_,th_,tbw,rows,l2w,l2h,data) in tiles:
            hdrs+=struct.pack('<HHHHHHBBHI',tx,ty,tw_,th_,tbw,rows,l2w,l2h,0,dpos+len(datas))
            datas+=data
            while len(datas)%16: datas+=b'\0'
        body+=hdrs
        while len(body)<dpos: body+=b'\0'
        body+=datas
        # alinea cada imagen a 16 bytes dentro del pak
        while off%16: off+=1; blobs.append(b'\0')
        entries.append(struct.pack('<IIHHBBBB',off,len(body),w,h,fmt,len(tiles),flip,0))
        blobs.append(bytes(body)); off+=len(body)
    with open(out,'wb') as f:
        f.write(b'PVZP'+struct.pack('<II',1,n))
        for e in entries: f.write(e)
        for b in blobs: f.write(b)
    print('pak',out,os.path.getsize(out),'bytes',n,'imagenes')
if __name__=='__main__': main(sys.argv[1],sys.argv[2])
