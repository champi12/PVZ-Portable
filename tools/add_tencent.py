#!/usr/bin/env python3
"""Anade el contenido del PvZ J2ME version Tencent (640x360) a gfx/anim:
   imagenes i0..i50 del Tencent -> ids 700.. (reescaladas x0.75 al tamano del J2ME 480x320)
   imagenes l*.png del Tencent  -> ids 1280+n (l.png = 1280, l37.png = 1317...), tambien x0.75
   animaciones /re del Tencent  -> archivos 52.. de anim.pak (posiciones x0.75, imagenes +700)
uso: add_tencent.py gfx_dir(J2ME+PvZBV, de make_gfx_dir) reanim_j2me.json tc_imgs/ tc_reanim.json jar_tc/ salida_gfx/ salida_reanim.json
     (tc_imgs y tc_reanim salen de extract_imgs.py / parse_reanim.py con las tablas del Tencent)"""
import sys, os, json, shutil
from PIL import Image
gdir, rj, timg, trj, jar, out, outj = sys.argv[1:8]
TC, LX, SC = 700, 1280, 0.75
os.makedirs(out, exist_ok=True)
meta = json.load(open(os.path.join(gdir, 'meta.json')))
for m in meta: shutil.copy(os.path.join(gdir, '%03d.png' % m['id']), os.path.join(out, '%03d.png' % m['id']))
have = {m['id'] for m in meta}
def put(idx, im):
    im.save(os.path.join(out, '%03d.png' % idx)); meta.append(dict(id=idx, w=im.width, h=im.height, flip=0)); have.add(idx)
def scaled(path):
    im = Image.open(path).convert('RGBA')
    ncol = len(im.getcolors(1 << 24) or [])
    w, h = max(1, round(im.width * SC)), max(1, round(im.height * SC))
    im = im.resize((w, h), Image.LANCZOS)
    if ncol <= 256: im = im.quantize(256, method=Image.FASTOCTREE).convert('RGBA')   # sigue cabiendo en T8
    return im
def fill(upto):
    for i in range(upto):
        if i not in have: put(i, Image.new('RGBA', (1, 1)))
tmeta = json.load(open(os.path.join(timg, 'meta.json')))
fill(TC)
for m in tmeta: put(TC + m['id'], scaled(os.path.join(timg, '%03d.png' % m['id'])))
fill(LX)
for n in range(0, 69):
    p = os.path.join(jar, ('l%d.png' % n) if n else 'l.png')
    if os.path.exists(p): put(LX + n, scaled(p))
# sobres de las plantas nuevas (orden PL_GATLING.. de defs.h): 1360+k a 47x33 (selector), 1380+k a 38x28 (barra)
NEWPK = [481, 176, 113, 80, 538, 291, 233, 473, 84]
fill(1360)
for k, i in enumerate(NEWPK):
    im = Image.open(os.path.join(timg, '%03d.png' % i)).convert('RGBA')
    put(1360 + k, im.resize((47, 33), Image.LANCZOS))
fill(1380)
for k, i in enumerate(NEWPK):
    im = Image.open(os.path.join(timg, '%03d.png' % i)).convert('RGBA')
    put(1380 + k, im.crop((3, 3, 65, 47)).resize((38, 28), Image.LANCZOS))
fill(max(have) + 1)
meta.sort(key=lambda m: m['id'])
json.dump(meta, open(os.path.join(out, 'meta.json'), 'w'), indent=0)
R = json.load(open(rj)); T = json.load(open(trj))
for r in T:
    for tr in r['tracks']:
        for f in tr:
            f[1] = round(f[1] * SC); f[2] = round(f[2] * SC)
            if f[7] >= 0: f[7] += TC
json.dump(R + T, open(outj, 'w'))
print(len(meta), 'imagenes,', len(R + T), 'animaciones')
