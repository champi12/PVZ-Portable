#!/usr/bin/env python3
"""Sobres de semillas a partir de la hoja de sobres del PC (8 columnas de 54x35, separadas 1 px).
Se les pega el globo blanco del coste (casilla 7,3 de la hoja) y el coste en amarillo con contorno negro,
como los sobres del J2ME, con una fuente de pixel propia (4x7 en el sobre grande, 3x7 en el pequeno)."""
from PIL import Image

class Sheet:
    def __init__(self, path):
        self.im = Image.open(path).convert('RGBA')
        self.bubble = self.cell(7, 3)
    def cell(self, r, c): return self.im.crop((1 + 55 * c, 1 + 36 * r, 55 + 55 * c, 36 + 36 * r))
    def packet(self, r, c, w, h, cost):
        base = self.cell(r, c).copy()
        if cost: base.alpha_composite(self.bubble)
        im = base.resize((w, h), Image.LANCZOS)
        if cost:
            cx = round(40.5 * w / 54); by1 = round(30 * h / 35)   # el globo ocupa x 29..50, y 17..29 de la hoja
            draw_cost(im, cost, None, by1 - 2, narrow=w < 40, center=cx)
        return im

FONT = {  # 4x7, el 2 con la parte de arriba redonda
 '0': ['.##.', '#..#', '#..#', '#..#', '#..#', '#..#', '.##.'],
 '1': ['.#..', '##..', '.#..', '.#..', '.#..', '.#..', '###.'],
 '2': ['.##.', '#..#', '...#', '..#.', '.#..', '#...', '####'],
 '3': ['###.', '...#', '...#', '.##.', '...#', '...#', '###.'],
 '4': ['#..#', '#..#', '#..#', '####', '...#', '...#', '...#'],
 '5': ['####', '#...', '###.', '...#', '...#', '#..#', '.##.'],
 '6': ['.##.', '#...', '###.', '#..#', '#..#', '#..#', '.##.'],
 '7': ['####', '...#', '..#.', '..#.', '.#..', '.#..', '.#..'],
 '8': ['.##.', '#..#', '#..#', '.##.', '#..#', '#..#', '.##.'],
 '9': ['.##.', '#..#', '#..#', '.###', '...#', '...#', '.##.'],
}
FONT3 = {  # 3x7 para el sobre pequeno
 '0': ['.#.', '#.#', '#.#', '#.#', '#.#', '#.#', '.#.'],
 '1': ['.#.', '##.', '.#.', '.#.', '.#.', '.#.', '###'],
 '2': ['.#.', '#.#', '..#', '..#', '.#.', '#..', '###'],
 '3': ['##.', '..#', '..#', '.#.', '..#', '..#', '##.'],
 '4': ['#.#', '#.#', '#.#', '###', '..#', '..#', '..#'],
 '5': ['###', '#..', '##.', '..#', '..#', '#.#', '.#.'],
 '6': ['.#.', '#..', '##.', '#.#', '#.#', '#.#', '.#.'],
 '7': ['###', '..#', '..#', '.#.', '.#.', '.#.', '.#.'],
 '8': ['.#.', '#.#', '#.#', '.#.', '#.#', '#.#', '.#.'],
 '9': ['.#.', '#.#', '#.#', '.##', '..#', '..#', '.#.'],
}
def draw_cost(im, cost, right, bottom, narrow=False, center=None):
    """digitos amarillos con contorno negro, como los sobres del J2ME (fuente propia 4x7; 3x7 en el pequeno)"""
    YEL, BLK = (255, 254, 25, 255), (0, 0, 0, 255)
    F = FONT3 if narrow else FONT; gw = 3 if narrow else 4
    w = len(cost) * (gw + 1) - 1; x0 = right - w if center is None else round(center - w / 2); y0 = bottom - 7; p = im.load()
    on = set()
    for k, d in enumerate(cost):
        for y, row in enumerate(F[d]):
            for x, ch in enumerate(row):
                if ch == '#': on.add((x0 + k * (gw + 1) + x, y0 + y))
    for (x, y) in on:
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                q = (x + dx, y + dy)
                if q not in on and 0 <= q[0] < im.width and 0 <= q[1] < im.height: p[q] = BLK
    for q in on: p[q] = YEL

def redraw_cost(im, cost, narrow):
    """cambia los digitos amarillos de un sobre del J2ME/PvZBV por los de esta fuente (el 2 del J2ME parecia una z)"""
    p = im.load(); W, H = im.size
    yel = lambda c: c[3] > 0 and c[0] > 180 and c[1] > 160 and c[2] < 120
    pts = [(x, y) for x in range(W - 24, W) for y in range(H - 14, H) if yel(p[x, y])]
    if not pts: return im
    x0 = min(a for a, b in pts); x1 = max(a for a, b in pts); y0 = min(b for a, b in pts); y1 = max(b for a, b in pts)
    hist = {}
    for x in range(x0 - 2, min(W, x1 + 3)):
        for y in range(y0 - 2, min(H, y1 + 3)):
            c = p[x, y]
            if c[3] == 255 and min(c[:3]) > 200: hist[c] = hist.get(c, 0) + 1
    white = max(hist, key=hist.get) if hist else (255, 255, 255, 255)
    for x in range(x0 - 1, min(W, x1 + 2)):
        for y in range(y0 - 1, min(H, y1 + 2)): p[x, y] = white
    draw_cost(im, cost, None, y1 + 1, narrow=narrow, center=(x0 + x1 + 1) / 2)
    return im
