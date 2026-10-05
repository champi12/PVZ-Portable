#!/usr/bin/env python3
"""Builds the PSP XMB icon (ICON0.PNG, 144x80) from the MIDlet icon inside the .jar.

Only the Python standard library is used: the 8-bit palette PNG is decoded by hand.
Usage: make_icon.py game.jar ICON0.PNG
"""
import struct
import sys
import zipfile
import zlib


def read_png(data):
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    p = 8
    idat = b''
    palette = []
    trns = b''
    while p < len(data):
        ln, typ = struct.unpack('>I4s', data[p:p + 8])
        body = data[p + 8:p + 8 + ln]
        p += 12 + ln
        if typ == b'IHDR':
            w, h, depth, ctype = struct.unpack('>IIBB', body[:10])
        elif typ == b'PLTE':
            palette = [tuple(body[i:i + 3]) for i in range(0, len(body), 3)]
        elif typ == b'tRNS':
            trns = body
        elif typ == b'IDAT':
            idat += body
        elif typ == b'IEND':
            break
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ctype]
    if depth != 8:
        raise ValueError('unsupported bit depth')
    raw = zlib.decompress(idat)
    stride = w * channels
    rows = []
    prev = bytearray(stride)
    pos = 0
    for _ in range(h):
        f = raw[pos]
        line = bytearray(raw[pos + 1:pos + 1 + stride])
        pos += 1 + stride
        for i in range(stride):
            a = line[i - channels] if i >= channels else 0
            b = prev[i]
            c = prev[i - channels] if i >= channels else 0
            if f == 1:
                line[i] = (line[i] + a) & 255
            elif f == 2:
                line[i] = (line[i] + b) & 255
            elif f == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 255
        rows.append(line)
        prev = line
    out = []
    for line in rows:
        row = []
        for x in range(w):
            if ctype == 3:
                idx = line[x]
                r, g, b = palette[idx]
                a = trns[idx] if idx < len(trns) else 255
            elif ctype == 6:
                r, g, b, a = line[x * 4:x * 4 + 4]
            elif ctype == 2:
                r, g, b = line[x * 3:x * 3 + 3]
                a = 255
            else:
                r = g = b = line[x * channels]
                a = 255
            row.append((r, g, b, a))
        out.append(row)
    return w, h, out


def write_png(path, w, h, pixels):
    raw = b''.join(b'\0' + bytes(c for px in row for c in px) for row in pixels)

    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xFFFFFFFF)

    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)))
        f.write(chunk(b'IDAT', zlib.compress(raw, 9)))
        f.write(chunk(b'IEND', b''))


def main():
    jar, out = sys.argv[1], sys.argv[2]
    with zipfile.ZipFile(jar) as z:
        w, h, src = read_png(z.read('i.png'))
    W, H = 144, 80
    scale = 2  # 32x32 -> 64x64, crisp pixel-art upscale
    canvas = [[(0, 0, 0, 0) for _ in range(W)] for _ in range(H)]
    ox, oy = (W - w * scale) // 2, (H - h * scale) // 2
    for y in range(h * scale):
        for x in range(w * scale):
            canvas[oy + y][ox + x] = src[y // scale][x // scale]
    write_png(out, W, H, canvas)


if __name__ == '__main__':
    main()
