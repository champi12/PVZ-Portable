#!/usr/bin/env python3
"""Lee las tablas de texto del PvZ J2ME (t-eng, t-spa...) = secuencia de cadenas Java UTF
(u16 longitud + UTF-8 modificado; en esta version vienen doble-codificadas)."""
import json, struct, sys
def read(path):
    d=open(path,'rb').read(); p=0; out=[]
    while p+2<=len(d):
        n=struct.unpack('>H',d[p:p+2])[0]; s=d[p+2:p+2+n]; p+=2+n
        s=s.replace(b'\xc0\x80',b'\x00').decode('utf-8','replace')
        try: s=s.encode('latin-1').decode('utf-8')
        except Exception: pass
        out.append(s)
    return out
if __name__=='__main__':
    eng=read(sys.argv[1]); spa=read(sys.argv[2])
    json.dump([{'id':i,'en':e,'es':s} for i,(e,s) in enumerate(zip(eng,spa))],open(sys.argv[3],'w'),ensure_ascii=False,indent=0)
    print(len(eng),len(spa))
