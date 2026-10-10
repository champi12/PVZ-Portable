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
            bx1 = round(50 * w / 54); by1 = round(30 * h / 35)   # el globo ocupa x 29..50, y 17..29 de la hoja
            draw_cost(im, cost, bx1 - 2, by1 - 2, narrow=w < 40)
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
def draw_cost(im, cost, right, bottom, narrow=False):
    """digitos amarillos con contorno negro, como los sobres del J2ME (fuente propia 4x7; 3x7 en el pequeno)"""
    YEL, BLK = (255, 254, 25, 255), (0, 0, 0, 255)
    F = FONT3 if narrow else FONT; gw = 3 if narrow else 4
    w = len(cost) * (gw + 1) - 1; x0 = right - w; y0 = bottom - 7; p = im.load()
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
