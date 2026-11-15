#!/usr/bin/env python3
"""bmp2png.py IN.bmp OUT.png - uncompressed 24/32-bit BMP to PNG, standard library only.

Used by `make shot` on Linux (macOS uses `sips`)."""
import struct
import sys
import zlib


def read_bmp(path):
    d = open(path, 'rb').read()
    if d[:2] != b'BM':
        sys.exit(f'{path}: not a BMP file')
    off = struct.unpack_from('<I', d, 10)[0]
    w, h, _, bpp, comp = struct.unpack_from('<iiHHI', d, 18)
    if bpp not in (24, 32) or comp not in (0, 3):
        sys.exit(f'{path}: only uncompressed 24/32-bit BMPs are supported (bpp={bpp}, compression={comp})')
    top_down = h < 0
    h = abs(h)
    step = bpp // 8
    stride = (w * step + 3) & ~3
    rows = []
    for y in range(h):
        src = y if top_down else h - 1 - y
        line = d[off + src * stride: off + src * stride + w * step]
        rgb = bytearray(w * 3)
        rgb[0::3] = line[2::step]   # BMP stores B, G, R(, A)
        rgb[1::3] = line[1::step]
        rgb[2::3] = line[0::step]
        rows.append(bytes(rgb))
    return w, h, rows


def write_png(path, w, h, rows):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    raw = b''.join(b'\0' + r for r in rows)
    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)))
        f.write(chunk(b'IDAT', zlib.compress(raw, 9)))
        f.write(chunk(b'IEND', b''))


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    write_png(sys.argv[2], *read_bmp(sys.argv[1]))
