#!/usr/bin/env python3
"""Parser del archivo /re (animaciones tipo Reanim del PvZ J2ME).
Estructura (por archivo i, desde ay.d[i] hasta ay.d[i+1]):
  u8 ntracks; i32 fps
  por pista: i32 nframes; por frame:
     i8 g (-1 oculto / 0 visible)
     6 campos (x, y, sx, sy, kx, ky) en orden a,b,e,f,c,d: u8 flag; si flag!=0 -> i32 valor (si no, repite el anterior)
     imagen: i8 flag; si != -1 -> i32 id de imagen (si no, repite)
Big endian."""
import struct, json, sys
D=[0,22613,33670,45595,52856,58729,65762,75771,89168,99505,108870,112523,119364,133037,142138,143675,151868,213409,214878,216359,218280,221469,224934,226631,230228,231261,231890,232183,233640,235553,236742,237443,238520,240169,241082,241727,243024,243353,243566,246943,247628,249693,250870,251963,253816,254249,254782,256059,256140,262829,262910,263223,263584]
def parse(buf):
    p=0
    def b():
        nonlocal p; v=struct.unpack('>b',buf[p:p+1])[0]; p+=1; return v
    def i():
        nonlocal p; v=struct.unpack('>i',buf[p:p+4])[0]; p+=4; return v
    nt=b(); fps=i(); tracks=[]
    for t in range(nt):
        nf=i(); frames=[]; last=[0,0,0,0,0,0]; img=-1
        for f in range(nf):
            g=b(); vals=[]
            for k in range(6):
                if b()!=0: last[k]=i()
                vals.append(last[k])
            fl=b()
            if fl!=-1: img=i()
            frames.append([g]+vals+[img])
        tracks.append(frames)
    return dict(fps=fps,tracks=tracks,end=p,size=len(buf))
if __name__=='__main__':
    re_=open(sys.argv[1],'rb').read(); out=[]
    for n in range(len(D)-1):
        r=parse(re_[D[n]:D[n+1]]); out.append(r)
        imgs=sorted({fr[7] for tr in r['tracks'] for fr in tr if fr[7]>=0})
        print(n,'fps',r['fps'],'tracks',len(r['tracks']),'frames',[len(t) for t in r['tracks']][:6],'ok',r['end']==r['size'],'imgs',imgs[:12])
    json.dump(out,open(sys.argv[2],'w'))
