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
