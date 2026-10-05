#!/usr/bin/env python3
"""Packs every non-class file of a MIDlet .jar into the resource blob read by runtime/resources.cpp."""
import struct
import sys
import zipfile


def main():
    jar, out = sys.argv[1], sys.argv[2]
    entries = []
    with zipfile.ZipFile(jar) as z:
        for n in sorted(z.namelist()):
            if n.endswith('/') or n.endswith('.class') or n.startswith('META-INF/'):
                continue
            entries.append((n.encode('utf-8'), z.read(n)))
    header = struct.pack('<I', len(entries))
    index_size = 4 + sum(2 + len(n) + 8 for n, _ in entries)
    off = (index_size + 15) & ~15
    index = b''
    blobs = b''
    for n, d in entries:
        index += struct.pack('<H', len(n)) + n + struct.pack('<II', off + len(blobs), len(d))
        blobs += d
        blobs += b'\0' * ((-len(blobs)) % 4)
    data = header + index
    data += b'\0' * (off - len(data))
    with open(out, 'wb') as f:
        f.write(data + blobs)
    print('packed %d resources (%d bytes)' % (len(entries), len(data) + len(blobs)))


if __name__ == '__main__':
    main()
