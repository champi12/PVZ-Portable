#!/usr/bin/env python3
"""Convierte las animaciones compiladas del PvZ de PC 1.0 (compiled/reanim/*.reanim.compiled, volcado de memoria
de 32 bits comprimido con zlib) al XML .reanim que lee PvZ-Portable.
uso: reanim_decompile.py <carpeta del juego>   -> escribe reanim/*.reanim"""
import sys, os, struct, zlib
UNSET = -10000.0
class R:
    def __init__(s, b): s.b = b; s.p = 0
    def i(s): v = struct.unpack_from('<i', s.b, s.p)[0]; s.p += 4; return v
    def raw(s, n): v = s.b[s.p:s.p + n]; s.p += n; return v
    def str(s): n = s.i(); return s.raw(n).decode('latin-1') if n else ''
def load(path):
    d = open(path, 'rb').read()
    cookie, size = struct.unpack_from('<II', d)
    assert cookie == 0xDEADFED4, 'cookie'
    b = zlib.decompress(d[8:]); assert len(b) == size
    r = R(b); r.i()                                   # hash del esquema
    top = r.raw(16); ntr = struct.unpack_from('<i', top, 4)[0]; fps = struct.unpack_from('<f', top, 8)[0]
    assert r.i() == 12
    traw = r.raw(12 * ntr)
    tracks = []
    for k in range(ntr):
        nt = struct.unpack_from('<i', traw, 12 * k + 8)[0]
        name = r.str()
        assert r.i() == 44
        xraw = r.raw(44 * nt)
        ts = []
        for j in range(nt):
            fl = struct.unpack_from('<8f', xraw, 44 * j)
            img = r.str(); font = r.str(); text = r.str()
            ts.append((fl, img, font, text))
        tracks.append((name, ts))
    assert r.p == len(b), 'sobran %d bytes' % (len(b) - r.p)
    return fps, tracks
def fmt(v):
    s = ('%.6f' % v).rstrip('0').rstrip('.')
    return s if s not in ('-0', '') else '0'
def to_xml(fps, tracks):
    o = ['<fps>%s</fps>' % fmt(fps)]
    for name, ts in tracks:
        o.append('<track>'); o.append('<name>%s</name>' % name)
        for fl, img, font, text in ts:
            t = '<t>'
            for tag, v in zip(('x', 'y', 'kx', 'ky', 'sx', 'sy', 'f', 'a'), fl):
                if v != UNSET: t += '<%s>%s</%s>' % (tag, fmt(v), tag)
            if img: t += '<i>%s</i>' % img
            if font: t += '<font>%s</font>' % font
            if text: t += '<text>%s</text>' % text
            o.append(t + '</t>')
        o.append('</track>')
    return '\n'.join(o) + '\n'
if __name__ == '__main__':
    g = sys.argv[1]; src = os.path.join(g, 'compiled', 'reanim'); n = 0
    for f in sorted(os.listdir(src)):
        if not f.endswith('.reanim.compiled'): continue
        fps, tracks = load(os.path.join(src, f))
        open(os.path.join(g, 'reanim', f[:-len('.compiled')]), 'w', encoding='latin-1').write(to_xml(fps, tracks)); n += 1
    print(n, 'animaciones convertidas')
