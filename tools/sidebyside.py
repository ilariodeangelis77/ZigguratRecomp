"""Compose two same-size PNGs (written by our converters: 8-bit RGB, filter 0) side by side.
Usage: python -I tools/sidebyside.py left.png right.png out.png"""
import struct, sys, zlib
def read(p):
    b = open(p, 'rb').read(); o = 8; idat = b''; w = h = 0
    while o < len(b):
        l = struct.unpack('>I', b[o:o+4])[0]; t = b[o+4:o+8]; d = b[o+8:o+8+l]; o += 12 + l
        if t == b'IHDR': w, h = struct.unpack('>II', d[:8])
        if t == b'IDAT': idat += d
    raw = zlib.decompress(idat); s = w * 3 + 1
    return w, h, [raw[y*s+1:(y+1)*s] for y in range(h)]
w1, h1, a = read(sys.argv[1]); w2, h2, b = read(sys.argv[2])
gap = b'\xff\x00\xff' * 4
rows = [b'\0' + a[y] + gap + b[y] for y in range(min(h1, h2))]
ch = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d))
open(sys.argv[3], 'wb').write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w1 + w2 + 4, min(h1, h2), 8, 2, 0, 0, 0)) + ch(b'IDAT', zlib.compress(b''.join(rows))) + ch(b'IEND', b''))
