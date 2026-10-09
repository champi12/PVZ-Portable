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
    w, h = max(1, round(im.width * SC)), max(1, round(im.height * SC))
    im = im.resize((w, h), Image.LANCZOS)
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
# sobres de las plantas nuevas (orden PL_GATLING.. de defs.h): 1360+k a 47x33 (selector), 1380+k a 38x28 (barra).
# El icono sale del sobre del Tencent y el coste se monta con los digitos de pixel de los sobres del J2ME/PvZBV
# (el del Tencent reducido no se leia).
NEWPK = [481, 176, 113, 80, 538, 291, 233, 473, 84]
COSTS = ['350', '350', '500', '225', '100', '25', '50', '125', '']
def yellow(px): return px[3] > 0 and px[0] > 180 and px[1] > 160 and px[2] < 120
def segs(box):
    cols = [any(yellow(box.getpixel((x, y))) for y in range(box.height)) for x in range(box.width)]
    out, x = [], 0
    while x < len(cols):
        if cols[x]:
            st = x
            while x < len(cols) and cols[x]: x += 1
            out.append((st, x - 1))
        else: x += 1
    return out
def boxes(lib_ids, costs, bw, bh):
    glyph, base = {}, {}
    for i, cs in zip(lib_ids, costs):
        im = Image.open(os.path.join(out, '%03d.png' % i)).convert('RGBA')
        box = im.crop((im.width - bw, im.height - bh, im.width, im.height))
        sg = segs(box)
        if len(sg) != len(cs): print('aviso: digitos', i, cs, sg); continue
        for d, (a, b) in zip(cs, sg): glyph.setdefault(d, box.crop((max(0, a - 1), 0, min(bw, b + 2), bh)))
        base.setdefault(len(cs), (box, sg))
    return glyph, base
def cost_box(cs, glyph, base):
    box, sg = base[len(cs)]; box = box.copy()
    hist = {}
    for x in range(box.width):
        for y in range(box.height):
            p = box.getpixel((x, y))
            if p[3] == 255 and min(p[:3]) > 200: hist[p] = hist.get(p, 0) + 1
    white = max(hist, key=hist.get) if hist else (255, 255, 255, 255)
    rows = [y for y in range(box.height) if any(yellow(box.getpixel((x, y))) for x in range(box.width))]
    y0, y1 = min(rows) - 1, max(rows) + 2
    for (a, b), d in zip(sg, cs):
        for x in range(max(0, a - 1), min(box.width, b + 2)):
            for y in range(max(0, y0), min(box.height, y1)): box.putpixel((x, y), white)
    for (a, b), d in zip(sg, cs):
        g = glyph[d]; box.paste(g, (min(box.width, b + 2) - g.width, 0), g)
    return box
fill(1360)
gb, bb = boxes([547, 14, 219, 467, 429, 241, 542], ['325', '50', '300', '175', '100', '125', '25'], 21, 12)
for k, i in enumerate(NEWPK):
    im = Image.open(os.path.join(timg, '%03d.png' % i)).convert('RGBA').resize((47, 33), Image.LANCZOS)
    if COSTS[k]: bx = cost_box(COSTS[k], gb, bb); im.paste(bx, (47 - bx.width, 33 - bx.height))
    put(1360 + k, im)
fill(1380)
gs, bs = boxes([620 + 18, 620 + 1, 620 + 30, 620 + 5, 620 + 0, 620 + 23, 620 + 4], ['325', '50', '300', '175', '100', '125', '25'], 21, 12)
for k, i in enumerate(NEWPK):
    im = Image.open(os.path.join(timg, '%03d.png' % i)).convert('RGBA').crop((3, 3, 65, 47)).resize((38, 28), Image.LANCZOS)
    if COSTS[k]: bx = cost_box(COSTS[k], gs, bs); im.paste(bx, (38 - bx.width, 28 - bx.height))
    put(1380 + k, im)
# 1390: nube de niebla generada (mancha suave con ruido, transparente en los bordes; no hay ninguna limpia en los jar)
import random
from PIL import ImageFilter
random.seed(7)
N = 96
noise = Image.new('L', (N, N))
noise.putdata([random.randint(0, 255) for _ in range(N * N)])
noise = noise.filter(ImageFilter.GaussianBlur(6))
px = noise.load(); lo = min(noise.getdata()); hi = max(noise.getdata())
al = Image.new('L', (N, N)); ap = al.load()
for y in range(N):
    for x in range(N):
        d = (((x - N / 2) ** 2 + (y - N / 2) ** 2) ** 0.5) / (N / 2)
        f = max(0.0, 1 - d) ** 1.6
        n = (px[x, y] - lo) / max(1, hi - lo)
        ap[x, y] = int(255 * f * (0.55 + 0.45 * n))
fog = Image.merge('RGBA', (Image.new('L', (N, N), 235), Image.new('L', (N, N), 235), Image.new('L', (N, N), 240), al))
put(1390, fog)
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
