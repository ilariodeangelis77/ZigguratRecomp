"""Convert port frame dumps (BMP, 24/32-bit) to PNG. Usage: python -I tools/bmp2png.py <files...>"""
import struct, sys, zlib


def conv(p):
    b = open(p, "rb").read()
    off = struct.unpack_from("<I", b, 10)[0]
    w, h = struct.unpack_from("<ii", b, 18)
    bpp = struct.unpack_from("<H", b, 28)[0]
    stride = (w * bpp // 8 + 3) & ~3
    rows = []
    for y in range(abs(h)):
        yy = abs(h) - 1 - y if h > 0 else y
        r = b[off + yy * stride: off + yy * stride + w * bpp // 8]
        step = bpp // 8
        rows.append(b"\0" + b"".join(bytes((r[i + 2], r[i + 1], r[i])) for i in range(0, len(r), step)))
    ch = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d))
    out = p[:-4] + ".png"
    open(out, "wb").write(b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", w, abs(h), 8, 2, 0, 0, 0))
                          + ch(b"IDAT", zlib.compress(b"".join(rows))) + ch(b"IEND", b""))
    return out


if __name__ == "__main__":
    for f in sys.argv[1:]:
        print(conv(f))
