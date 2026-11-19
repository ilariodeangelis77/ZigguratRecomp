"""Compare a port frame (BMP from --shot) with a touchHLE frame dump (portrait PPM, rotated ccw).
Usage: python -I tools/imgdiff.py port.bmp touchhle.ppm [diff.png]
Prints % of pixels whose max channel difference > 24 and the mean absolute difference."""
import struct, sys, zlib


def load_bmp(p):
    b = open(p, "rb").read()
    off = struct.unpack_from("<I", b, 10)[0]
    w, h = struct.unpack_from("<ii", b, 18)
    bp = struct.unpack_from("<H", b, 28)[0] // 8
    stride = (w * bp + 3) & ~3
    px = []
    for y in range(abs(h)):
        yy = abs(h) - 1 - y if h > 0 else y
        r = b[off + yy * stride: off + yy * stride + w * bp]
        px.append([(r[i + 2], r[i + 1], r[i]) for i in range(0, w * bp, bp)])
    return px


def load_ppm_rot(p):
    b = open(p, "rb").read()
    parts = b.split(b"\n", 3)
    w, h = map(int, parts[1].split())
    d = parts[3]
    nw, nh = h, w
    out = []
    for y in range(nh):
        row = []
        for x in range(nw):
            i = (x * w + (w - 1 - y)) * 3
            row.append((d[i], d[i + 1], d[i + 2]))
        out.append(row)
    return out


a = load_bmp(sys.argv[1])
b = load_ppm_rot(sys.argv[2])
H, W = min(len(a), len(b)), min(len(a[0]), len(b[0]))
diff = 0
tot = 0
rows = []
for y in range(H):
    r = bytearray(b"\0")
    for x in range(W):
        pa, pb = a[y][x], b[y][x]
        m = max(abs(pa[k] - pb[k]) for k in range(3))
        tot += sum(abs(pa[k] - pb[k]) for k in range(3)) / 3
        if m > 24:
            diff += 1
            r += bytes((255, 0, 255))
        else:
            g = sum(pa) // 3 // 3
            r += bytes((g, g, g))
    rows.append(bytes(r))
print(f"{diff * 100.0 / (W * H):.2f}% pixels differ, mean abs diff {tot / (W * H):.2f}")
if len(sys.argv) > 3:
    ch = lambda t, dd: struct.pack(">I", len(dd)) + t + dd + struct.pack(">I", zlib.crc32(t + dd))
    open(sys.argv[3], "wb").write(b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0))
                                   + ch(b"IDAT", zlib.compress(b"".join(rows))) + ch(b"IEND", b""))
