#!/usr/bin/env python3
"""Piezas de la seta melancolica (hoja de sprites de la DS en ref/melancoseta_ds.png): cada pieza esta en un
recuadro con fondo blanco sobre fondo rosa; se recortan y el blanco que toca el borde se vuelve transparente.
uso: gloom_parts.py hoja.png salida_dir   -> g00.png.. (cabeza, mofletes, ojos, bocas de tubo, cuerpo)"""
import sys, os
from PIL import Image
BOXES = [(1, 9, 84, 77), (85, 9, 155, 62), (156, 9, 226, 62), (227, 9, 287, 30), (288, 9, 349, 30),
         (1, 78, 24, 96), (25, 78, 49, 100), (50, 78, 65, 99),
         (1, 101, 30, 122), (31, 101, 53, 126), (54, 101, 67, 127), (68, 101, 95, 128), (96, 101, 128, 122),
         (1, 139, 72, 181)]
def cut(im, b):
    c = im.crop(b).convert('RGBA'); p = c.load(); W, H = c.size
    white = lambda q: min(p[q][:3]) > 235
    st = [(x, y) for x in range(W) for y in (0, H - 1)] + [(x, y) for y in range(H) for x in (0, W - 1)]
    seen = set()
    while st:
        q = st.pop()
        if q in seen or not (0 <= q[0] < W and 0 <= q[1] < H) or not white(q): continue
        seen.add(q); p[q] = (0, 0, 0, 0)
        x, y = q; st += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    for q in list(seen):                                        # borde suave: blanquecinos junto al hueco
        x, y = q
        for n in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if 0 <= n[0] < W and 0 <= n[1] < H and n not in seen and min(p[n][:3]) > 200 and p[n][3]:
                r, g, b_, a = p[n]; p[n] = (r, g, b_, 140)
    return c.crop(c.getbbox()) if c.getbbox() else c
if __name__ == '__main__':
    im = Image.open(sys.argv[1]); os.makedirs(sys.argv[2], exist_ok=True)
    for k, b in enumerate(BOXES): cut(im, b).save(os.path.join(sys.argv[2], 'g%02d.png' % k))

# Seta melancolica armada como la del PC: cuerpo verde + 8 tubos alrededor (detras de la cabeza).
# Devuelve (imagen, cx, cy): cx,cy = donde va el centro de la cabeza dentro de la imagen.
# (pieza, dx, dy, espejo) relativo al centro de la cabeza, en pixeles de la hoja
TUBES = [(7, -40, 0, 0), (10, -45, 0, 0), (7, 40, 0, 1), (10, 45, 0, 1),            # lados: tramo + boca
         (6, -32, 24, 0), (9, -38, 29, 0), (6, 32, 24, 1), (9, 38, 29, 1),           # abajo en diagonal
         (5, 0, 34, 0), (8, 0, 41, 0),                                               # abajo de frente
         (11, -31, -28, 0), (11, 31, -28, 1), (12, 0, -40, 0)]                       # arriba
def assemble(im):
    parts = [cut(im, b) for b in BOXES]
    W, H, cx, cy = 140, 130, 70, 56
    out = Image.new('RGBA', (W, H))
    body = parts[13]
    out.alpha_composite(body, (cx - body.width // 2, cy + 26))
    for k, dx, dy, fl in TUBES:
        p = parts[k].transpose(Image.FLIP_LEFT_RIGHT) if fl else parts[k]
        out.alpha_composite(p, (cx + dx - p.width // 2, cy + dy - p.height // 2))
    bb = out.getbbox(); out = out.crop(bb)
    return out, cx - bb[0], cy - bb[1]
