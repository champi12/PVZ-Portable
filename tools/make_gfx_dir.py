#!/usr/bin/env python3
"""Junta en una carpeta todas las imagenes de data/gfx.pak:
   0..604   imagenes del PvZ J2ME 4.6.0 (extract_imgs.py)
   605..611 pantalla de carga del J2ME (l.png, l1..l6.png del jar)
   612..    piezas del PvZBV (version de teclado): cursor, marco de semillas, flechas y sobres pequenos
uso: make_gfx_dir.py imgs_j2me/ jar_460/ imgs_pvzbv/ salida/   (luego make_pak.py salida data/gfx.pak)"""
import sys, os, json, shutil
from PIL import Image
src, jar, bv, out = sys.argv[1:5]
os.makedirs(out, exist_ok=True)
meta = json.load(open(os.path.join(src, 'meta.json')))[:605]
for m in meta: shutil.copy(os.path.join(src, '%03d.png' % m['id']), os.path.join(out, '%03d.png' % m['id']))
def add(idx, path):
    im = Image.open(path).convert('RGBA'); im.save(os.path.join(out, '%03d.png' % idx))
    meta.append(dict(id=idx, w=im.width, h=im.height, flip=0))
for k, n in enumerate(['l', 'l1', 'l2', 'l3', 'l4', 'l5', 'l6']): add(605 + k, os.path.join(jar, n + '.png'))
# PvZBV: 612 esquina del cursor, 613 esquina roja (no se puede plantar), 614 marco del sobre,
# 615/616 flechas amarillas del menu, 617 marco pequeno
for k, i in enumerate([45, 543, 93, 196, 197, 78]): add(612 + k, os.path.join(bv, '%03d.png' % i))
add(618, os.path.join(bv, '212.png'))      # candado (niveles bloqueados)
add(619, os.path.join(bv, '078.png'))      # sin uso
# 620.. sobres 38x28 del PvZBV en el orden de las plantas del port (PL_*)
pk = [275, 382, 266, 80, 370, 116, 137, 571, 328, 85, 222, 578, 86, 200, 395, 56, 82, 23, 161, 480, 207, 129, 422, 191, 426, 389, 32, 498, 198, 443, 549]
for k, i in enumerate(pk): add(620 + k, os.path.join(bv, '%03d.png' % i))
# 651.. logos del PvZBV: frances, aleman, italiano
for k, i in enumerate([215, 386, 324]): add(651 + k, os.path.join(bv, '%03d.png' % i))
json.dump(meta, open(os.path.join(out, 'meta.json'), 'w'), indent=0)
print(len(meta), 'imagenes')
