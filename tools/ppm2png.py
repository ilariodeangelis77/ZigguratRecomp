"""Convert touchHLE frame dumps (portrait PPM) to landscape PNGs.
Usage: python -I tools/ppm2png.py <dir> [--rot cw|ccw|none] [names...]"""
import glob, os, struct, sys, zlib


def conv(p, rot):
    b = open(p, "rb").read()
    parts = b.split(b"\n", 3)
    w, h = map(int, parts[1].split())
    px = parts[3]
    if rot == "none":
        nw, nh = w, h
        get = lambda x, y: (y * w + x) * 3
    elif rot == "ccw":
        nw, nh = h, w
        get = lambda x, y: (x * w + (w - 1 - y)) * 3
    else:  # cw
        nw, nh = h, w
        get = lambda x, y: ((h - 1 - x) * w + y) * 3
    rows = []
    for y in range(nh):
        r = bytearray(b"\0")
        for x in range(nw):
            i = get(x, y)
            r += px[i:i + 3]
        rows.append(bytes(r))
    ch = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d))
    out = p[:-4] + ".png"
    open(out, "wb").write(b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", nw, nh, 8, 2, 0, 0, 0))
                          + ch(b"IDAT", zlib.compress(b"".join(rows))) + ch(b"IEND", b""))
    return out


if __name__ == "__main__":
    d = sys.argv[1]
    rot = "ccw"
    names = []
    args = sys.argv[2:]
    while args:
        a = args.pop(0)
        if a == "--rot":
            rot = args.pop(0)
        else:
            names.append(a)
    files = [os.path.join(d, n) for n in names] if names else sorted(glob.glob(os.path.join(d, "*.ppm")))
    for f in files:
        print(conv(f, rot))
