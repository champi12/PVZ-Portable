#!/usr/bin/env python3
"""Junta los .sfx (IMA-ADPCM) en data/sfx.pak y genera src/sfx_ids.h"""
import sys, os, struct
src, out, hdr = sys.argv[1:4]
names=sorted(f[:-4] for f in os.listdir(src) if f.endswith('.sfx'))
blobs=[open(os.path.join(src,n+'.sfx'),'rb').read() for n in names]
off=8+8*len(names)
with open(out,'wb') as f:
    f.write(b'SFXP'+struct.pack('<I',len(names)))
    for b in blobs: f.write(struct.pack('<II',off,len(b))); off+=len(b)
    for b in blobs: f.write(b)
with open(hdr,'w') as h:
    h.write('/* Generado por tools/make_sfxpak.py - no editar */\n#ifndef SFX_IDS_H\n#define SFX_IDS_H\nenum {\n')
    for i,n in enumerate(names): h.write('    SFX_%s = %d,\n'%(n.upper(),i))
    h.write('    SFX_COUNT = %d\n};\n#endif\n'%len(names))
print(len(names),'sfx ->',out,os.path.getsize(out))
