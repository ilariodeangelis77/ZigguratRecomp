"""Contact sheet of BMP frames (pure Python). Usage: python -I tools/contact.py out.png cols scale f1.bmp f2.bmp ..."""
import struct, sys, zlib


def readbmp(p):
    b = open(p, "rb").read()
    off = struct.unpack_from("<I", b, 10)[0]
    w, h = struct.unpack_from("<ii", b, 18)
    bpp = struct.unpack_from("<H", b, 28)[0]
    st = (w * bpp // 8 + 3) & ~3
    rows = []
    for y in range(abs(h)):
        yy = abs(h) - 1 - y if h > 0 else y
        r = b[off + yy * st: off + yy * st + w * bpp // 8]
        rows.append([(r[i + 2], r[i + 1], r[i]) for i in range(0, len(r), bpp // 8)])
    return w, abs(h), rows


def main():
    out, cols, sc = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    tiles = [readbmp(f) for f in sys.argv[4:]]
    tw, th = tiles[0][0] // sc, tiles[0][1] // sc
    nrows = (len(tiles) + cols - 1) // cols
    W, H = cols * tw, nrows * th
    img = [[(0, 0, 0)] * W for _ in range(H)]
    for k, (w, h, px) in enumerate(tiles):
        ox, oy = (k % cols) * tw, (k // cols) * th
        for y in range(th):
            for x in range(tw):
                img[oy + y][ox + x] = px[y * sc][x * sc]
    raw = b"".join(b"\0" + bytes(c for p in row for c in p) for row in img)
    ch = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d))
    open(out, "wb").write(b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0))
                          + ch(b"IDAT", zlib.compress(raw)) + ch(b"IEND", b""))


main()
