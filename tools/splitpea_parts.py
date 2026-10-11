#!/usr/bin/env python3
"""Guisantralla (split pea) armada con las piezas de ref/guisantralla_partes.png como la del PC.
assemble(im) -> (cabeza, cuerpo): la cabeza doble (boca atras, cabeza de atras, cabeza principal, boca
delante) y el cuerpo (tallo + hojas). Coordenadas de la hoja; la cabeza se une al cuerpo en HEAD_AT."""
from PIL import Image
from gloom_parts import cut
BOXES = [(1, 9, 56, 59), (57, 9, 127, 74), (128, 9, 163, 58), (164, 9, 186, 20), (1, 85, 29, 108), (30, 85, 58, 108),
         (1, 117, 23, 146), (24, 117, 91, 157), (92, 117, 107, 148), (108, 117, 123, 129), (124, 117, 168, 139),
         (169, 117, 181, 127), (1, 166, 27, 183), (28, 166, 43, 185)]
# (pieza, x, y del centro, espejo, escala) en un lienzo de cabeza de 180x80 / cuerpo de 120x96
HEAD = [(2, 22, 40, 1, 1.05), (0, 60, 40, 0, 1.15), (3, 52, 8, 0, 1.0), (1, 108, 40, 0, 1.05), (2, 152, 40, 0, 1.05)]
BODY = [(10, 60, 54, 0, 1.9), (13, 61, 22, 0, 2.2), (7, 60, 60, 0, 1.7)]
def _canvas(parts, items, W, H):
    out = Image.new('RGBA', (W, H))
    for k, x, y, fl, sc in items:
        p = parts[k].transpose(Image.FLIP_LEFT_RIGHT) if fl else parts[k]
        if sc != 1: p = p.resize((round(p.width * sc), round(p.height * sc)), Image.LANCZOS)
        out.alpha_composite(p, (x - p.width // 2, y - p.height // 2))
    return out
def assemble(im):
    parts = [cut(im, b) for b in BOXES]
    from PIL import ImageEnhance
    for k in (6, 7, 8, 9, 10, 11):         # hojas un poco mas oscuras, como las del PC (si no se pierden en el cesped)
        a = parts[k].split()[3]; parts[k] = ImageEnhance.Brightness(parts[k].convert('RGB')).enhance(0.8).convert('RGBA'); parts[k].putalpha(a)
    return _canvas(parts, HEAD, 180, 80), _canvas(parts, BODY, 120, 92), parts
HEAD_JOINT = (108, 72)   # donde el tallo entra en la cabeza (lienzo de la cabeza)
BODY_TOP = (60, 0)       # arriba del tallo (lienzo del cuerpo)
