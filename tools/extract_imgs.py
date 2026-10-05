# Extrae las 605 imagenes del paquete i0..i45 del PvZ J2ME
import io, json, os, struct, zlib
from PIL import Image
blob=b''.join(open(f'jar/i{i}','rb').read() for i in range(46))
pal=open('jar/p','rb').read()
pos=0
def rd(n):
    global pos; v=blob[pos:pos+n]; pos+=n; return v
def u(n): return int.from_bytes(rd(n),'big')
def s(n):
    v=u(n); 
    return v-(1<<(8*n)) if v>>(8*n-1) else v
# tabla de paletas (af.a())
npal=pal[0]; poff=[]; p=1
for i in range(npal):
    poff.append(p)
    a,b,c_,d=[int.from_bytes(pal[p+1+2*k:p+3+2*k],'big') for k in range(4)]
    p+=9+(a<<1)+(b<<2)+c_*5+d*6

def apply_pal(img, n5):
    """Aproxima af.a(): cambios de PLTE/tRNS segun paleta n5."""
    if n5<0 or img.mode!='P': return img
    q=poff[n5]; delta=pal[q]; n26,n25,n24,n23=[int.from_bytes(pal[q+1+2*k:q+3+2*k],'big') for k in range(4)]
    q+=9
    plt=bytearray(img.getpalette() or b'')
    plt += bytes(768-len(plt))
    trns=img.info.get('transparency')
    tr=bytearray(trns) if isinstance(trns,(bytes,bytearray)) else None
    # tRNS: delta + n26 pares (idx,alpha)
    if tr is not None:
        tr=bytearray(tr)+bytes(256-len(tr))
        if delta:
            sd=delta-256 if delta>127 else delta
            for i in range(256): tr[i]=max(0,min(255,tr[i]+sd))
    k=q
    for i in range(n26):
        if tr is not None: tr[pal[k]]=pal[k+1]
        k+=2
    for i in range(n25):
        idx=pal[k]; plt[idx*3:idx*3+3]=pal[k+1:k+4]; k+=4
    for i in range(n24):
        idx=pal[k]; plt[idx*3:idx*3+3]=pal[k+1:k+4]
        if tr is not None: tr[idx]=pal[k+4]
        k+=5
    for i in range(n23):
        src=pal[k:k+3]; dst=pal[k+3:k+6]; k+=6
        for j in range(256):
            if plt[j*3:j*3+3]==src: plt[j*3:j*3+3]=dst
    img=img.copy(); img.putpalette(bytes(plt))
    if tr is not None: img.info['transparency']=bytes(tr)
    return img

def flags_fx(img, fl):
    if fl & 0x70 or fl & 8 or fl & 4:
        im=img.convert('RGBA'); r,g,b,a=im.split(); m=fl&0x70
        if m==80: r,b=b,r
        elif m==64: r,g,b=b,r,g
        elif m==48: r,g,b=g,b,r
        elif m==32: r,g=g,r
        elif m==16: g,b=b,g
        im=Image.merge('RGBA',(r,g,b,a))
        if fl&8:
            from PIL import ImageOps
            l=ImageOps.grayscale(im.convert('RGB')); im=Image.merge('RGBA',(l,l,l,a))
        if fl&4:
            from PIL import ImageChops
            rgb=ImageChops.invert(im.convert('RGB')); im=Image.merge('RGBA',(*rgb.split(),a))
        return im
    return img

os.makedirs('imgs',exist_ok=True)
meta=[]; c=0; g=0
def save(idx, im, fl, n8, n9, grp, derived):
    im2=apply_pal(im,n8)
    im2=flags_fx(im2,fl & ~3)
    im2.convert('RGBA').save(f'imgs/{idx:03d}.png')
    meta.append(dict(id=idx,w=im2.width,h=im2.height,flip=fl&3,flags=fl,pal=n8,x=n9,group=grp,derived=derived))

while c<605:
    rd(8); L=u(4); data=rd(L)
    base=Image.open(io.BytesIO(data)); base.load()
    n=u(2)
    for i in range(n):
        rd(8); fl=u(1); n8=u(2); n9=u(1)
        save(c, base, fl, -1 if n8==65535 else n8, n9, g, None); c+=1
    nd=u(2)
    for d in range(nd):
        rd(8); nops=u(1); im=base; ops=[]
        for k in range(nops):
            t=u(1)
            if t==0:
                x,y,w,h=u(2),u(2),u(2),u(2); im=im.crop((x,y,x+w,y+h)); ops.append(('crop',x,y,w,h))
            elif t==1:
                sn=s(4); cs=s(4)
                import math
                ang=math.degrees(math.atan2(sn,cs))
                im=im.convert('RGBA').rotate(ang,expand=True,resample=Image.NEAREST) if im.mode!='P' else im.rotate(ang,expand=True)
                ops.append(('rot',round(ang,2)))
            else:
                sx,sy=u(2),u(2); im=im.resize((max(1,im.width*sx//100),max(1,im.height*sy//100)),Image.NEAREST); ops.append(('scale',sx,sy))
        m=u(2)
        for i in range(m):
            rd(8); fl=u(1); n8=u(2); n9=u(1)
            save(c, im, fl, -1 if n8==65535 else n8, n9, g, ops); c+=1
    g+=1
json.dump(meta,open('imgs/meta.json','w'),indent=0)
print('groups',g,'images',c,'end',pos,len(blob))
