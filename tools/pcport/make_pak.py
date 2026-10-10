#!/usr/bin/env python3
"""Empaqueta carpetas de datos de PvZ en un main.pak (formato PopCap: XOR 0xF7).

Uso: make_pak.py SALIDA.pak DIR_RAIZ carpeta1 [carpeta2 ...]
Ejemplo: make_pak.py main.pak "PvZ 1.0" images reanim particles sounds compiled data
"""
import os, struct, sys

def main():
    out, root, dirs = sys.argv[1], sys.argv[2], sys.argv[3:]
    files = []
    for d in dirs:
        for base, _, names in os.walk(os.path.join(root, d)):
            for n in sorted(names):
                p = os.path.join(base, n)
                files.append((os.path.relpath(p, root).replace('/', '\\'), p))
    head = bytearray(struct.pack('<II', 0xBAC04AC0, 0))
    for name, p in files:
        nb = name.encode('latin-1')
        head += struct.pack('<BB', 0, len(nb)) + nb + struct.pack('<iq', os.path.getsize(p), 0)
    head += b'\x80'
    table = bytes(i ^ 0xF7 for i in range(256))
    xor = lambda b: b.translate(table)
    with open(out, 'wb') as f:
        f.write(xor(bytes(head)))
        for _, p in files:
            with open(p, 'rb') as g:
                while True:
                    b = g.read(1 << 20)
                    if not b:
                        break
                    f.write(xor(b))
    print(len(files), 'archivos ->', out)

if __name__ == '__main__':
    main()
