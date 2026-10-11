#!/usr/bin/env python3
"""Recorta la hoja de sprites del lanzaguisantes de la DS (la que tiene los sobres a la izquierda y 5 filas de
fotogramas a la derecha) en imagenes sueltas para tools/add_pc.py (@archivo).

Cada fotograma se recorta con su celda entera (las rayas negras de debajo marcan las celdas), asi todos comparten
el mismo punto de apoyo: abajo, a 14.5 px del borde izquierdo de la celda (DS_PEA_AX en src/board.c).

uso: cut_ds_peashooter.py hoja.png carpeta_salida
     escribe ds_pea_idle0-7 (reposo), ds_pea_shoot0-3 (disparo), ds_pea (el guisante) y ds_peasplat (los 3
     fotogramas del golpe en una tira de 16 px). Las versiones naranjas y las dos filas de abajo son de un minijuego
     y no se usan."""
import sys, os
from PIL import Image

X0 = 102              # a la izquierda estan los sobres
ROWS = [(0, 32), (66, 102)]   # (arriba, raya de debajo) de las filas verdes: reposo y disparo


def cells(a, y):
    """celdas de una fila: los tramos de la raya negra de debajo"""
    seg, cur = [], None
    for x in range(X0, a.width + 1):
        on = x < a.width and a.getpixel((x, y))[3] > 0
        if on and cur is None:
            cur = x
        if not on and cur is not None:
            if x - cur > 4:
                seg.append((cur, x))
            cur = None
    return seg


def main():
    a = Image.open(sys.argv[1]).convert('RGBA')
    out = sys.argv[2]
    os.makedirs(out, exist_ok=True)
    names = [['idle%d' % i for i in range(8)],
             ['shoot%d' % i for i in range(4)] + ['-', '-', '-'] + ['pea', 'splat0', 'splat1', 'splat2']]
    splats = []
    for (y0, y1), nm in zip(ROWS, names):
        cs = [c for c in cells(a, y1) if c[1] - c[0] < 40]          # sin el recuadro del texto
        for (x0, x1), n in zip(cs, nm):
            if n == '-':
                continue
            im = a.crop((x0, y0, x1, y1))
            if n == 'pea' or n.startswith('splat'):
                im = im.crop(im.getbbox())
            if n.startswith('splat'):
                splats.append(im)
                continue
            im.save(os.path.join(out, 'ds_pea_%s.png' % n if n != 'pea' else 'ds_pea.png'))
    strip = Image.new('RGBA', (16 * len(splats), 16))
    for i, im in enumerate(splats):
        strip.paste(im, (16 * i + (16 - im.width) // 2, (16 - im.height) // 2))
    strip.save(os.path.join(out, 'ds_peasplat.png'))
    print('fotogramas en', out)


if __name__ == '__main__':
    main()
