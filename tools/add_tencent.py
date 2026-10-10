#!/usr/bin/env python3
"""Anade el contenido del PvZ J2ME version Tencent (640x360) a gfx/anim:
   imagenes i0..i50 del Tencent -> ids 700.. (reescaladas x0.75 al tamano del J2ME 480x320)
   imagenes l*.png del Tencent  -> ids 1280+n (l.png = 1280, l37.png = 1317...), tambien x0.75
   animaciones /re del Tencent  -> archivos 52.. de anim.pak (posiciones x0.75, imagenes +700)
uso: add_tencent.py gfx_dir(J2ME+PvZBV, de make_gfx_dir) reanim_j2me.json tc_imgs/ tc_reanim.json jar_tc/ salida_gfx/ salida_reanim.json [hoja_sobres_pc.png]
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
NEWPK = [481, 176, 113, 80, 538, 291, 233, 473, 59, 84]           # 59 (seta) solo de relleno: la melancolica sale de la hoja
COSTS = ['350', '350', '500', '225', '100', '25', '50', '125', '150', '']
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
# --- sobre de la espadana dibujado de nuevo al estilo del J2ME (el del Tencent reducido se ve mal):
# marco y degradado de cielo/cesped de un sobre del J2ME, la planta en primer plano (su animacion del Tencent,
# frame de reposo, a resolucion nativa) y el coste con los digitos de pixel del J2ME
import math
TCJ = json.load(open(trj))
def render_tc(n, frame):
    r = TCJ[n]; W = 240; c = Image.new('RGBA', (W, W), (0, 0, 0, 0))
    for tr in r['tracks']:
        g, x, y, sx, sy, kx, ky, img = tr[min(frame, len(tr) - 1)]
        if g == -1 or img < 0: continue
        sx = (sx or 4096) / 4096; sy = (sy or 4096) / 4096
        ax = -kx * math.pi / 180 / 4096; ay = -ky * math.pi / 180 / 4096
        ma, mb, mc, md = math.cos(ax) * sx, -math.sin(ax) * sx, math.sin(ay) * sy, math.cos(ay) * sy
        det = ma * md - mc * mb
        if abs(det) < 1e-6: continue
        ia, ib, ic, id_ = md / det, -mc / det, -mb / det, ma / det
        tx, ty = x / 4096 + 80, y / 4096 + 80
        src = Image.open(os.path.join(timg, '%03d.png' % img)).convert('RGBA')
        c.alpha_composite(src.transform((W, W), Image.AFFINE, (ia, ib, -ia * tx - ib * ty, ic, id_, -ic * tx - id_ * ty), resample=Image.BICUBIC))
    return c.crop(c.getbbox())
def j2me_packet(base_id, grad_id, plant, w, h, pw, dx, dy, fw=4):
    """marco del sobre base_id; fondo = degradado de la columna 4 del sobre grad_id (la maceta deja ver el cielo
    y el cesped); la planta escalada a pw px de ancho, abajo y algo cortada por el marco, como en el J2ME"""
    base = Image.open(os.path.join(out, '%03d.png' % base_id)).convert('RGBA')
    grad = Image.open(os.path.join(out, '%03d.png' % grad_id)).convert('RGBA')
    x0, y0, x1, y1 = fw, fw, w - 1 - fw, h - 1 - fw   # interior (el marco ocupa fw px)
    pk = base.copy(); px = pk.load()
    bp = base.load()
    for y in range(h):                               # marco limpio: la mitad derecha es la izquierda reflejada
        for x in range(w // 2, w):
            if y < y0 or x > x1: px[x, y] = bp[w - 1 - x, y]
    gh = grad.height
    for y in range(y0, y1 + 1):
        col = grad.getpixel((4, min(gh - 6, max(4, round(4 + (y - y0) * (gh - 10) / max(1, y1 - y0))))))
        for x in range(x0, x1 + 1):
            k = 1 - 0.06 * abs((x - x0) / max(1, x1 - x0) - 0.5)     # un poco mas claro en el centro
            px[x, y] = tuple(min(255, round(c * k + (1 - k) * 255)) for c in col[:3]) + (255,)
    sc = pw / plant.width
    pl = plant.resize((round(plant.width * sc), round(plant.height * sc)), Image.LANCZOS)
    layer = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    layer.paste(pl, (round((w - pl.width) / 2 + dx), round(y1 + 1 - pl.height + dy)), pl)
    mask = Image.new('L', (w, h), 0); mask.paste(255, (x0, y0, x1 + 1, y1 + 1))
    layer.putalpha(Image.composite(layer.getchannel('A'), Image.new('L', (w, h), 0), mask))
    pk.alpha_composite(layer)
    return pk
CATTAIL = 3
cat = render_tc(24, 7)
print('espadana', cat.size)
pk = j2me_packet(195, 362, cat, 47, 33, 38, -3, -3)
bx = cost_box('225', gb, bb); pk.paste(bx, (47 - bx.width, 33 - bx.height))
meta[:] = [m for m in meta if m['id'] != 1360 + CATTAIL]; put(1360 + CATTAIL, pk)
fill(1380)
gs, bs = boxes([620 + 18, 620 + 1, 620 + 30, 620 + 5, 620 + 0, 620 + 23, 620 + 4], ['325', '50', '300', '175', '100', '125', '25'], 21, 12)
for k, i in enumerate(NEWPK):
    im = Image.open(os.path.join(timg, '%03d.png' % i)).convert('RGBA').crop((3, 3, 65, 47)).resize((38, 28), Image.LANCZOS)
    if COSTS[k]: bx = cost_box(COSTS[k], gs, bs); im.paste(bx, (38 - bx.width, 28 - bx.height))
    if k == CATTAIL:
        im = j2me_packet(647, 362, cat, 38, 28, 31, -3, -2, 3)
        bx = cost_box('225', gs, bs); im.paste(bx, (38 - bx.width, 28 - bx.height))
    put(1380 + k, im)
# hoja de sobres del PC (opcional, 8o argumento): sustituye los sobres de las plantas nuevas y anade los de las
# plantas que aun no estan en el juego (1400+ a 47x33, 1440+ a 38x28; orden de EXTRA)
if len(sys.argv) > 8:
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from seed_sheet import Sheet
    sh = Sheet(sys.argv[8])
    NEWCELL = [(5, 0), (5, 4), (5, 7), (5, 3), (3, 3), (3, 1), (4, 4), (3, 6), (5, 2), (6, 1)]   # orden de NEWPK
    for k, (r, c) in enumerate(NEWCELL):
        for base, (w, h) in ((1360, (47, 33)), (1380, (38, 28))):
            meta[:] = [m for m in meta if m['id'] != base + k]; have.discard(base + k)
            put(base + k, sh.packet(r, c, w, h, COSTS[k]))
    # guisante partido, seta iman, grano de cafe, hoja paraguas, maravilla, girasol gemelo, seta melancolica,
    # iman dorado, pinchoroca, imitador, nuez gigante
    EXTRA = [((3, 4), '125'), ((3, 7), '100'), ((4, 3), '75'), ((4, 5), '100'), ((4, 6), '50'), ((5, 1), '150'),
             ((5, 2), '150'), ((5, 5), '50'), ((5, 6), '125'), ((6, 0), '0'), ((6, 2), '')]
    fill(1400)
    for k, ((r, c), cost) in enumerate(EXTRA): put(1400 + k, sh.packet(r, c, 47, 33, cost))
    fill(1440)
    for k, ((r, c), cost) in enumerate(EXTRA): put(1440 + k, sh.packet(r, c, 38, 28, cost))
# extras de ref/ (opcionales, junto a la hoja de sobres): 1460.. piezas de la seta melancolica (hoja de la DS,
# reducidas a 0.42) y 1474 la pua del cactus/espadana
if len(sys.argv) > 8:
    ref = os.path.dirname(os.path.abspath(sys.argv[8]))
    gp = os.path.join(ref, 'melancoseta_ds.png')
    if os.path.exists(gp):
        from gloom_parts import BOXES, cut
        gim = Image.open(gp)
        fill(1460)
        for k, b in enumerate(BOXES):
            c = cut(gim, b); put(1460 + k, c.resize((max(1, round(c.width * 0.42)), max(1, round(c.height * 0.42))), Image.LANCZOS))
    sp = os.path.join(ref, 'pua_cactus.png')
    if os.path.exists(sp):
        fill(1474); sim = Image.open(sp).convert('RGBA')
        put(1474, sim.resize((round(sim.width * 0.6), round(sim.height * 0.6)), Image.LANCZOS))   # al tamano de las demas puas
    gp = os.path.join(ref, 'melancoseta_ds.png')
    if os.path.exists(gp):                     # 1475: cuerpo y tubos de la seta melancolica armados (piezas DS)
        from gloom_parts import assemble
        a, acx, acy = assemble(Image.open(gp))
        fill(1475); put(1475, a.resize((round(a.width * 0.42), round(a.height * 0.42)), Image.LANCZOS))
        print('seta melancolica: centro de la cabeza en', round(acx * 0.42, 1), round(acy * 0.42, 1), 'de', a.size)
    dp = os.path.join(ref, 'ds_menus.webp')
    if os.path.exists(dp):
        # pergamino de la DS: se separa el trazo (girasol / zombi) del papel y se pinta sobre un papel liso de un solo
        # tono, a menor tamano. 1476/1477 fondos del almanaque (446x212), 1480 recuadro de planta nueva (400x120)
        from PIL import ImageChops, ImageFilter as IFl, ImageStat
        dim = Image.open(dp).convert('RGB')
        inks = []
        for x0 in (1140, 1302):
            pg = dim.crop((x0 + 3, 274, x0 + 153, 363))
            bl = pg.filter(IFl.GaussianBlur(4))
            ink = ImageChops.subtract(bl, pg).convert('L').point(lambda v: 0 if v < 6 else min(255, (v - 3) * 3))
            inks.append(Image.merge('RGB', (ink, ink, ink)))
        paper = tuple(int(v) for v in ImageStat.Stat(dim.crop((1200, 280, 1290, 300))).median)
        def sheet(w, h, ink, sc, x, y):
            out = Image.new('RGB', (w, h), paper)
            ik = ink.resize((round(ink.width * sc), round(ink.height * sc)), Image.LANCZOS)
            layer = Image.new('RGB', (w, h)); layer.paste(ik, (x, y))
            return ImageChops.subtract(out, layer).convert('RGBA')
        fill(1476)
        put(1476, sheet(446, 212, inks[0], 2.2, 4, 212 - 196))
        put(1477, sheet(446, 212, inks[1], 2.2, 4, 212 - 196))
    sp = os.path.join(ref, 'guisantralla_partes.png')
    if os.path.exists(sp):                     # 1478/1479: guisantralla armada (cabeza doble / tallo y hojas)
        from splitpea_parts import assemble as sp_assemble
        hd, bd, _ = sp_assemble(Image.open(sp))
        fill(1478)
        put(1478, hd.resize((44, 22), Image.LANCZOS)); put(1479, bd.resize((32, 24), Image.LANCZOS))
    if os.path.exists(dp):
        fill(1480); put(1480, sheet(400, 120, inks[0], 1.3, 2, 120 - 116))
    zp = os.path.join(ref, 'zombi_partes.png')
    if os.path.exists(zp):                     # 1481: cara de asco del zombi al morder el ajo (hoja del PC)
        from gloom_parts import cut
        zim = Image.open(zp)
        k = 19 / cut(zim, (1, 9, 54, 57)).width   # al tamano de la cabeza del J2ME (78: 19 px)
        y = cut(zim, (303, 9, 356, 85))
        fill(1481); put(1481, y.resize((round(y.width * k), round(y.height * k)), Image.LANCZOS))
    cp = os.path.join(ref, 'cafe_partes.png')
    if os.path.exists(cp):                     # 1482: grano de cafe en tira de 22 frames de 16x49 (10 quieto + 12 deshaciendose)
        cim = Image.open(cp).convert('RGBA')
        def nowhite(b):
            c = cim.crop(b); px = c.load(); W, H = c.size
            st = [(x, y) for x in range(W) for y in (0, H - 1)] + [(x, y) for y in range(H) for x in (0, W - 1)]; seen = set()
            while st:
                q = st.pop()
                if q in seen or not (0 <= q[0] < W and 0 <= q[1] < H) or min(px[q][:3]) < 225: continue
                seen.add(q); px[q] = (0, 0, 0, 0); x, y = q; st += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
            return c
        heads = [nowhite((1 + 58 * k, 9, 58 + 58 * k, 165)) for k in range(12)]
        leaf = nowhite((1, 174, 33, 201)); leaf = leaf.crop(leaf.getbbox())
        frames = []
        bb = heads[0].getbbox(); a0 = heads[0].split()[3].load()
        xs = [x for x in range(bb[0], bb[2]) if a0[x, bb[1] + 2] > 60]; tx = (xs[0] + xs[-1]) // 2      # punta de arriba del grano
        lf = leaf.transpose(Image.FLIP_LEFT_RIGHT).rotate(28, expand=True, resample=Image.BICUBIC)   # base abajo a la derecha, punta arriba a la izquierda
        lf = lf.resize((round(lf.width * 0.7), round(lf.height * 0.7)), Image.LANCZOS)
        for k in range(12):
            f = Image.new('RGBA', (57, 156 + 18)); f.alpha_composite(heads[k], (0, 18))
            if k < 3:                          # la hoja arriba a la izquierda mientras el grano esta entero (como el PC)
                l2 = lf if k == 0 else lf.point(lambda v: v)
                f.alpha_composite(l2, (max(0, tx + 4 - l2.width), max(0, bb[1] + 18 + 6 - l2.height)))
            frames.append(f.resize((16, 49), Image.LANCZOS))
        seq = [frames[0]] * 10 + frames[1:] + [frames[11]]
        strip = Image.new('RGBA', (16 * len(seq), 49))
        for k, f in enumerate(seq): strip.alpha_composite(f, (16 * k, 0))
        fill(1482); put(1482, strip)
    bp = os.path.join(ref, 'cinta_marco.png')
    if os.path.exists(bp):                     # 1483/1484: cinta transportadora del PC (marco y banda) a la altura de la pantalla
        fr = Image.open(bp).convert('RGBA'); bt = Image.open(os.path.join(ref, 'cinta.png')).convert('RGBA')
        fill(1483); put(1483, fr.resize((50, 272), Image.LANCZOS)); put(1484, bt.resize((44, 88), Image.LANCZOS))
    mp = os.path.join(ref, 'mazo.png')
    if os.path.exists(mp): fill(1485); put(1485, Image.open(mp).convert('RGBA'))   # 1485: mazo de golpea al zombi
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
        ap[x, y] = max(1, int(255 * f * (0.55 + 0.45 * n)))   # nunca 0: el bilineal de la PSP mezcla con negro
fog = Image.merge('RGBA', (Image.new('L', (N, N), 235), Image.new('L', (N, N), 235), Image.new('L', (N, N), 240), al))
meta[:] = [m for m in meta if m['id'] != 1390]; have.discard(1390)
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
