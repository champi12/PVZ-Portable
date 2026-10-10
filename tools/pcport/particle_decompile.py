#!/usr/bin/env python3
"""Convierte las particulas y estelas compiladas del PvZ de PC 1.0 (compiled/particles/*.compiled, volcado de
memoria de 32 bits) al XML que lee PvZ-Portable.  uso: particle_decompile.py <carpeta del juego>"""
import sys, os, struct, zlib
CURVES = {1: None, 2: 'EaseIn', 3: 'EaseOut', 4: 'EaseInOut', 5: 'EaseInOutWeak', 6: 'FastInOut', 7: 'FastInOutWeak',
          9: 'Bounce', 10: 'BounceFastMiddle', 11: 'BounceSlowMiddle', 12: 'SinWave', 13: 'EaseSinWave'}
PFLAGS = ['RandomLaunchSpin', 'AlignLaunchSpin', 'AlignToPixel', 'SystemLoops', 'ParticleLoops', 'ParticlesDontFollow',
          'RandomStartTime', 'DieIfOverloaded', 'Additive', 'FullScreen', 'SoftwareOnly', 'HardwareOnly']
ETYPES = ['Circle', 'Box', 'BoxPath', 'CirclePath', 'CircleEvenSpacing']
FTYPES = {1: 'Friction', 2: 'Acceleration', 3: 'Attractor', 4: 'MaxVelocity', 5: 'Velocity', 6: 'Position',
          7: 'SystemPosition', 8: 'GroundConstraint', 9: 'Shake', 10: 'Circle', 11: 'Away'}
class R:
    def __init__(s, b): s.b = b; s.p = 0
    def i(s): v = struct.unpack_from('<i', s.b, s.p)[0]; s.p += 4; return v
    def raw(s, n): v = s.b[s.p:s.p + n]; s.p += n; return v
    def str(s): n = s.i(); return s.raw(n).decode('latin-1').rstrip('\0') if n else ''
    def track(s):
        n = s.i(); return [struct.unpack_from('<3f2i', s.raw(20)) for _ in range(n)]
def fmt(v):
    s = ('%.4f' % v).rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s
def track_xml(nodes):
    out = []
    for t, lo, hi, curve, dist in nodes:
        if lo == hi:
            s = '%s,%s' % (fmt(lo), fmt(t * 100))
            if CURVES.get(curve): s += ' ' + CURVES[curve]
        else:
            s = '[%s%s %s],%s' % (fmt(lo), (' ' + CURVES[dist]) if CURVES.get(dist) else '', fmt(hi), fmt(t * 100))
        out.append(s)
    return ' '.join(out)
def unpack(path):
    d = open(path, 'rb').read()
    assert struct.unpack_from('<I', d)[0] == 0xDEADFED4
    b = zlib.decompress(d[8:]); assert len(b) == struct.unpack_from('<I', d, 4)[0]
    r = R(b); r.i(); return r
EMIT_TRACKS1 = ['SystemDuration', 'CrossFadeDuration', 'SpawnRate', 'SpawnMinActive', 'SpawnMaxActive', 'SpawnMaxLaunched',
                'EmitterRadius', 'EmitterOffsetX', 'EmitterOffsetY', 'EmitterBoxX', 'EmitterBoxY', 'EmitterPath',
                'EmitterSkewX', 'EmitterSkewY', 'ParticleDuration', 'SystemRed', 'SystemGreen', 'SystemBlue', 'SystemAlpha',
                'SystemBrightness', 'LaunchSpeed', 'LaunchAngle']
EMIT_TRACKS2 = ['ParticleRed', 'ParticleGreen', 'ParticleBlue', 'ParticleAlpha', 'ParticleBrightness', 'ParticleSpinAngle',
                'ParticleSpinSpeed', 'ParticleScale', 'ParticleStretch', 'CollisionReflect', 'CollisionSpin', 'ClipTop',
                'ClipBottom', 'ClipLeft', 'ClipRight', 'AnimationRate']
def fields(r, count):
    assert r.i() == 20
    raw = r.raw(20 * count); out = []
    for k in range(count):
        ft = struct.unpack_from('<i', raw, 20 * k)[0]
        x = r.track(); y = r.track()
        s = '<FieldType>%s</FieldType>' % FTYPES.get(ft, 'Friction')
        if x: s += '<x>%s</x>' % track_xml(x)
        if y: s += '<y>%s</y>' % track_xml(y)
        out.append(s)
    return out
def particle(path):
    r = unpack(path)
    top = r.raw(8); n = struct.unpack_from('<i', top, 4)[0]
    assert r.i() == 356, 'tam emisor'
    raw = r.raw(356 * n); xml = []
    for k in range(n):
        e = raw[356 * k: 356 * (k + 1)]
        col, row, frames, anim, flags, etype = struct.unpack_from('<6i', e, 4)
        npf = struct.unpack_from('<i', e, 212 + 4)[0]; nsf = struct.unpack_from('<i', e, 220 + 4)[0]
        img = r.str(); name = r.str()
        o = ['<Emitter>']
        if name: o.append('<Name>%s</Name>' % name)
        if img: o.append('<Image>%s</Image>' % img)
        if col: o.append('<ImageCol>%d</ImageCol>' % col)
        if row: o.append('<ImageRow>%d</ImageRow>' % row)
        if frames != 1: o.append('<ImageFrames>%d</ImageFrames>' % frames)
        if anim: o.append('<Animated>%d</Animated>' % anim)
        for bit, fname in enumerate(PFLAGS):
            if flags & (1 << bit): o.append('<%s>1</%s>' % (fname, fname))
        if etype != 1: o.append('<EmitterType>%s</EmitterType>' % ETYPES[etype])
        # orden de lectura = orden del DefMap
        order = ['SystemDuration', '#OnDuration', 'CrossFadeDuration', 'SpawnRate', 'SpawnMinActive', 'SpawnMaxActive',
                 'SpawnMaxLaunched', 'EmitterRadius', 'EmitterOffsetX', 'EmitterOffsetY', 'EmitterBoxX', 'EmitterBoxY',
                 'EmitterPath', 'EmitterSkewX', 'EmitterSkewY', 'ParticleDuration', 'SystemRed', 'SystemGreen', 'SystemBlue',
                 'SystemAlpha', 'SystemBrightness', 'LaunchSpeed', 'LaunchAngle', '@Field', '@SystemField'] + EMIT_TRACKS2
        for f in order:
            if f == '#OnDuration':
                s = r.str()
                if s: o.append('<OnDuration>%s</OnDuration>' % s)
            elif f[0] == '@':
                for fx in fields(r, npf if f == '@Field' else nsf): o.append('<%s>%s</%s>' % (f[1:], fx, f[1:]))
            else:
                t = r.track()
                if t: o.append('<%s>%s</%s>' % (f, track_xml(t), f))
        o.append('</Emitter>'); xml.append('\n'.join(o))
    assert r.p == len(r.b), 'sobran %d' % (len(r.b) - r.p)
    return '\n'.join(xml) + '\n'
def trail(path):
    r = unpack(path)
    raw = r.raw(56)
    maxp, mind, flags = struct.unpack_from('<ifi', raw, 4)
    img = r.str(); o = []
    if img: o.append('<Image>%s</Image>' % img)
    o.append('<MaxPoints>%d</MaxPoints>' % maxp); o.append('<MinPointDistance>%s</MinPointDistance>' % fmt(mind))
    if flags & 1: o.append('<Loops>1</Loops>')
    for f in ['WidthOverLength', 'WidthOverTime', 'AlphaOverLength', 'AlphaOverTime', 'TrailDuration']:
        t = r.track()
        if t: o.append('<%s>%s</%s>' % (f, track_xml(t), f))
    assert r.p == len(r.b), 'sobran %d' % (len(r.b) - r.p)
    return '\n'.join(o) + '\n'
if __name__ == '__main__':
    g = sys.argv[1]; src = os.path.join(g, 'compiled', 'particles'); n = 0
    for f in sorted(os.listdir(src)):
        out = os.path.join(g, 'particles', f[:-len('.compiled')])
        x = particle(os.path.join(src, f)) if f.endswith('.xml.compiled') else trail(os.path.join(src, f))
        open(out, 'w', encoding='latin-1').write(x); n += 1
    print(n, 'particulas/estelas convertidas')
