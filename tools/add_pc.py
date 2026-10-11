#!/usr/bin/env python3
"""Anade animaciones del PvZ de PC (archivos .reanim y sus piezas) al juego de la PSP.

Las piezas se reducen a la escala del J2ME (una casilla del PC mide 80x100 y una del J2ME 30x37: x0.375) y se
anaden a gfx.pak; las animaciones se pasan al formato del J2ME (mismas pistas, posicion, escala e inclinacion por
frame) y se anaden a anim.pak a continuacion de las que ya hay. Ademas se escribe un .h con, por cada animacion,
su numero, el indice de cada pista y el rango de frames de las pistas de control (anim_idle, anim_shooting...).

uso: add_pc.py gfx.pak anim.pak carpeta_pc salida_gfx.pak salida_anim.pak pc_anims.h Nombre1 [Nombre2 ...]
     carpeta_pc = datos del PvZ de PC con reanim/*.reanim y sus imagenes (p. ej. el main.pak de la GOTY
     extraido); Nombre = archivo sin extension (SplitPea, PeaShooterSingle, Zombie...); +Imagen = una imagen
     suelta de reanim/ (p. ej. +Zombie_balloon_outerarm_upper2, el brazo roto) -> #define PC_IMG_<IMAGEN>;
     @ruta.png = una imagen tal cual, sin escalar ni retocar (los fotogramas de la DS de tools/cut_ds_peashooter.py)
     -> #define PC_IMG_<NOMBRE>"""
import sys, os, re, struct, math, json, tempfile
from PIL import Image, ImageEnhance
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_pak import convert

SC = 0.375          # PC -> J2ME
FIRST_IMG = 1500    # ids de las piezas del PC en gfx.pak

# estilo del J2ME: los graficos del movil son mas cabezones, las plantas algo mas grandes, con contorno oscuro,
# colores planos y bordes sin transparencias. Se aplica a las piezas y a las pistas al convertirlas.
ZHEAD = re.compile(r'head|hat|hair|tongue|jaw|lips|snorkle$|propeller|eye', re.I)   # pistas de la cabeza (zombis)
PHEAD = re.compile(r'face|head|mouth|blink|eyebrow', re.I)                         # (plantas con cabeza)
HEADED_PLANTS = {'SplitPea', 'PeaShooterSingle', 'PeaShooter', 'SnowPea', 'Repeater', 'Threepeater', 'GatlingPea'}
PLANT_SCALE = 1.15     # planta entera (alrededor de su base)
PLANT_HEAD = 1.2       # cabeza de las plantas
ZOMBIE_HEAD = 1.3      # cabeza de los zombis
COLORS = 32            # colores por pieza


def j2me_style(im):
    """pieza ya reducida -> estilo J2ME: mas color, colores planos, borde duro y contorno oscuro de 1 px"""
    im = im.convert('RGBA')
    a = im.getchannel('A').point(lambda v: 255 if v >= 110 else 0)
    rgb = ImageEnhance.Color(im.convert('RGB')).enhance(1.25)
    rgb = ImageEnhance.Contrast(rgb).enhance(1.1)
    rgb = rgb.quantize(COLORS, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE).convert('RGB')
    w, h = im.size
    A, P = a.load(), rgb.load()
    for y in range(h):
        for x in range(w):
            if not A[x, y]:
                continue
            edge = x == 0 or y == 0 or x == w - 1 or y == h - 1 or not (A[x - 1, y] and A[x + 1, y] and A[x, y - 1] and A[x, y + 1])
            if edge:
                r, g, b = P[x, y]
                P[x, y] = (r * 50 // 100, g * 50 // 100, b * 50 // 100)
    out = rgb.convert('RGBA')
    out.putalpha(a)
    return out


def read_gfx(path):
    d = open(path, 'rb').read()
    assert d[:4] == b'PVZP'
    n = struct.unpack_from('<I', d, 8)[0]
    ents = [struct.unpack_from('<IIHHBBBB', d, 12 + 16 * i) for i in range(n)]
    imgs = []
    for (off, size, w, h, fmt, nt, flip, _) in ents:
        imgs.append((w, h, fmt, nt, flip, d[off:off + size]))
    return imgs


def write_gfx(path, imgs):
    n = len(imgs)
    off = 12 + n * 16
    entries, blobs = [], []
    for (w, h, fmt, nt, flip, body) in imgs:
        pad = (-off) % 16
        if pad:
            blobs.append(b'\0' * pad)
            off += pad
        entries.append(struct.pack('<IIHHBBBB', off, len(body), w, h, fmt, nt, flip, 0))
        blobs.append(body)
        off += len(body)
    with open(path, 'wb') as f:
        f.write(b'PVZP' + struct.pack('<II', 1, n))
        for e in entries:
            f.write(e)
        for b in blobs:
            f.write(b)


def pack_image(im):
    """una imagen PIL -> (w, h, fmt, ntiles, flip, cuerpo) como make_pak"""
    with tempfile.NamedTemporaryFile(suffix='.png', delete=False) as t:
        im.save(t.name)
        w, h, fmt, flip, clut, tiles = convert(t.name, 0)
    os.unlink(t.name)
    body = bytearray(clut)
    dpos = (len(body) + len(tiles) * 20 + 15) & ~15
    hdrs, datas = bytearray(), bytearray()
    for (tx, ty, tw_, th_, tbw, rows, l2w, l2h, data) in tiles:
        hdrs += struct.pack('<HHHHHHBBHI', tx, ty, tw_, th_, tbw, rows, l2w, l2h, 0, dpos + len(datas))
        datas += data
        while len(datas) % 16:
            datas += b'\0'
    body += hdrs
    while len(body) < dpos:
        body += b'\0'
    body += datas
    return (w, h, fmt, len(tiles), flip, bytes(body))


def read_anim(path):
    d = open(path, 'rb').read()
    assert d[:4] == b'ANIM'
    n = struct.unpack_from('<I', d, 4)[0]
    offs = struct.unpack_from('<%dI' % (n + 1), d, 8)
    return [d[offs[i]:offs[i + 1]] for i in range(n)]


def write_anim(path, blobs):
    off = 8 + 4 * (len(blobs) + 1)
    offs = []
    for b in blobs:
        offs.append(off)
        off += len(b)
    offs.append(off)
    with open(path, 'wb') as f:
        f.write(b'ANIM' + struct.pack('<I', len(blobs)) + struct.pack('<%dI' % len(offs), *offs))
        for b in blobs:
            f.write(b)


def parse_reanim(path):
    s = open(path, encoding='latin-1').read()
    fps = int(re.search(r'<fps>(\d+)</fps>', s).group(1)) if '<fps>' in s else 12
    tracks = []
    for name, body in re.findall(r'<track>\s*<name>(.*?)</name>(.*?)</track>', s, re.S):
        cur = dict(x=0.0, y=0.0, sx=1.0, sy=1.0, kx=0.0, ky=0.0, f=0, a=1.0, i='')
        frames = []
        for t in re.findall(r'<t>(.*?)</t>', body, re.S):
            for k, v in re.findall(r'<(\w+)>(.*?)</\1>', t):
                if k in ('x', 'y', 'sx', 'sy', 'kx', 'ky', 'a'):
                    cur[k] = float(v)
                elif k == 'f':
                    cur['f'] = int(float(v))
                elif k == 'i':
                    cur['i'] = v
            frames.append(dict(cur))
        tracks.append((name, frames))
    return fps, tracks


def find_image(pcdir, name):
    """IMAGE_REANIM_PEASHOOTER_HEAD -> reanim/PeaShooter_head.png (sin distinguir mayusculas)"""
    key = name[len('IMAGE_REANIM_'):].lower() if name.upper().startswith('IMAGE_REANIM_') else name.lower()
    for sub in ('reanim', 'images/reanim', 'images'):
        d = os.path.join(pcdir, sub)
        if not os.path.isdir(d):
            continue
        for fn in os.listdir(d):
            stem, ext = os.path.splitext(fn)
            if stem.lower() == key and ext.lower() in ('.png', '.jpg'):
                return os.path.join(d, fn)
    return None


def main():
    gin, ain, pcdir, gout, aout, hout = sys.argv[1:7]
    names = sys.argv[7:]
    imgs = read_gfx(gin)
    anims = read_anim(ain)
    while len(imgs) < FIRST_IMG:
        imgs.append(pack_image(Image.new('RGBA', (1, 1))))
    imgid = {}
    sizes = {}
    hdr = ['/* generado por tools/add_pc.py: animaciones del PvZ de PC (indices de pista y rangos de frames) */',
           '#ifndef PC_ANIMS_H', '#define PC_ANIMS_H', '#define RE_PC_FIRST %d' % len(anims)]
    for nm in [n for n in names if n.startswith('@')]:
        stem = os.path.splitext(os.path.basename(nm[1:]))[0]
        hdr.append('#define PC_IMG_%s %d' % (re.sub(r'[^A-Z0-9]', '_', stem.upper()), len(imgs)))
        imgs.append(pack_image(Image.open(nm[1:]).convert('RGBA')))
    for nm in [n for n in names if n.startswith('+')]:
        p = find_image(pcdir, nm[1:])
        if not p:
            sys.exit('no encuentro la imagen ' + nm[1:])
        src = Image.open(p).convert('RGBA')
        sm = j2me_style(src.resize((max(1, round(src.width * SC)), max(1, round(src.height * SC))), Image.LANCZOS))
        hdr.append('#define PC_IMG_%s %d' % (re.sub(r'[^A-Z0-9]', '_', nm[1:].upper()), len(imgs)))
        imgs.append(pack_image(sm))
    for nm in [n for n in names if n[0] not in '+@']:
        path = os.path.join(pcdir, 'reanim', nm + '.reanim')
        fps, tracks = parse_reanim(path)
        nf = max(len(fr) for _, fr in tracks)
        for _, frames in tracks:
            for f in frames:
                im = f['i']
                if im and im not in imgid:
                    p = find_image(pcdir, im)
                    if not p:
                        print('aviso: no encuentro', im)
                        imgid[im] = -1
                        continue
                    src = Image.open(p).convert('RGBA')
                    w, h = max(1, round(src.width * SC)), max(1, round(src.height * SC))
                    sm = src.resize((w, h), Image.LANCZOS)
                    sm = j2me_style(sm)
                    imgid[im] = len(imgs)
                    sizes[imgid[im]] = (w, h)
                    imgs.append(pack_image(sm))
        # caja del frame de reposo (rango de anim_idle si existe; si no, el primer frame visible)
        def ctrl_range(tn):
            for name, frames in tracks:
                if name == tn:
                    v = [k for k, f in enumerate(frames) if f['f'] != -1]
                    return (v[0], v[-1]) if v else None
            return None
        f0 = (ctrl_range('anim_idle') or (0, 0))[0]
        # cuellos (abajo en el centro de la pieza de la cabeza) en cada frame, para agrandar la cabeza
        pivots = [[] for _ in range(nf)]
        if nm.startswith('Zombie') or nm in HEADED_PLANTS:
            for name, frames in tracks:
                for k in range(nf):
                    f = frames[min(k, len(frames) - 1)]
                    main = name == 'anim_head1' if nm.startswith('Zombie') else bool(re.search(r'_HEAD$', f['i'] or ''))
                    if not main or f['f'] == -1 or imgid.get(f['i'], -1) < 0:
                        continue
                    W, H = sizes[imgid[f['i']]]
                    kx, ky = -f['kx'] * math.pi / 180, -f['ky'] * math.pi / 180
                    a, b = math.cos(kx) * f['sx'], -math.sin(kx) * f['sx']
                    c, d = math.sin(ky) * f['sy'], math.cos(ky) * f['sy']
                    pivots[k].append((f['x'] * SC + a * W / 2 + c * H, f['y'] * SC + b * W / 2 + d * H))
        xs, ys = [], []
        blob = bytearray()
        for name, frames in tracks:
            for k in range(nf):
                f = frames[min(k, len(frames) - 1)]
                img = imgid.get(f['i'], -1) if f['i'] else -1
                vis = 1 if f['f'] != -1 and f['a'] > 0.05 else 0
                x, y = f['x'] * SC, f['y'] * SC
                sx, sy = f['sx'], f['sy']
                kx, ky = -f['kx'] * math.pi / 180, -f['ky'] * math.pi / 180
                zombie = nm.startswith('Zombie')
                hf = 1
                if pivots[k] and (ZHEAD if zombie else PHEAD).search(name):
                    # toda la cabeza (boca, ojos...) crece junta alrededor del cuello de la cabeza mas cercana
                    hf = ZOMBIE_HEAD if zombie else PLANT_HEAD
                    px, py = min(pivots[k], key=lambda q: (q[0] - x) ** 2 + (q[1] - y) ** 2)
                    x = px + (x - px) * hf; y = py + (y - py) * hf
                    sx *= hf; sy *= hf
                if not zombie:                      # planta entera mas grande, alrededor de su base (40, 80 del PC)
                    bx, by = 40 * SC, 80 * SC
                    x = bx + (x - bx) * PLANT_SCALE; y = by + (y - by) * PLANT_SCALE
                    sx *= PLANT_SCALE; sy *= PLANT_SCALE
                blob += struct.pack('<6fhbB', x, y, sx, sy, kx, ky, img, vis, 0)
                if k == f0 and vis and img >= 0:
                    W, H = sizes[img]
                    a, b = math.cos(kx) * sx, -math.sin(kx) * sx
                    c, d = math.sin(ky) * sy, math.cos(ky) * sy
                    for u, v in ((0, 0), (W, 0), (0, H), (W, H)):
                        xs.append(x + a * u + c * v)
                        ys.append(y + b * u + d * v)
        bbox = (min(xs), min(ys), max(xs), max(ys)) if xs else (0, 0, 0, 0)
        rid = len(anims)
        anims.append(b'RNM1' + struct.pack('<HHHH', len(tracks), nf, fps, 0) + struct.pack('<4f', *bbox) + bytes(blob))
        tag = re.sub(r'[^A-Z0-9]', '_', nm.upper())
        hdr.append('')
        hdr.append('#define RE_PC_%s %d   /* %s.reanim: %d pistas, %d frames, %d fps */' % (tag, rid, nm, len(tracks), nf, fps))
        for t, (name, frames) in enumerate(tracks):
            tn = re.sub(r'[^A-Z0-9]', '_', name.upper())
            hdr.append('#define PC_%s_T_%s %d' % (tag, tn, t))
            if all(not f['i'] for f in frames):        # pista de control: rango de frames
                r = ctrl_range(name)
                if r:
                    hdr.append('#define PC_%s_%s_S %d' % (tag, tn, r[0]))
                    hdr.append('#define PC_%s_%s_E %d' % (tag, tn, r[1]))
        print('%s -> animacion %d (%d pistas, %d frames)' % (nm, rid, len(tracks), nf))
    hdr.append('#define RE_PC_COUNT %d' % len(anims))
    hdr.append('#endif')
    write_gfx(gout, imgs)
    write_anim(aout, anims)
    open(hout, 'w').write('\n'.join(hdr) + '\n')
    print(len(imgs), 'imagenes,', len(anims), 'animaciones')


if __name__ == '__main__':
    main()
