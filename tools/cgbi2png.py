"""Convert Apple CgBI ("crushed") PNGs to standard PNGs. Pure Python, no deps.
CgBI: extra CgBI chunk before IHDR, IDAT is raw deflate (no zlib header),
pixels are BGRA with premultiplied alpha."""
import struct, sys, zlib, os


def chunks(b):
    o = 8
    while o < len(b):
        l = struct.unpack(">I", b[o:o + 4])[0]
        t = b[o + 4:o + 8]
        yield t, b[o + 8:o + 8 + l]
        o += 12 + l


def mk(t, d):
    return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)


def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def convert(src, dst):
    b = open(src, "rb").read()
    cs = list(chunks(b))
    if not any(t == b"CgBI" for t, _ in cs):
        open(dst, "wb").write(b)
        return "std"
    ihdr = [d for t, d in cs if t == b"IHDR"][0]
    w, h, depth, ctype, comp, filt, inter = struct.unpack(">IIBBBBB", ihdr)
    assert depth == 8 and ctype == 6 and inter == 0, (src, depth, ctype, inter)
    raw = zlib.decompress(b"".join(d for t, d in cs if t == b"IDAT"), -15)
    bpp, stride = 4, w * 4
    out = bytearray()
    prev = bytearray(stride)
    o = 0
    for y in range(h):
        f = raw[o]; o += 1
        line = bytearray(raw[o:o + stride]); o += stride
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            up = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if f == 1: line[i] = (line[i] + a) & 255
            elif f == 2: line[i] = (line[i] + up) & 255
            elif f == 3: line[i] = (line[i] + ((a + up) >> 1)) & 255
            elif f == 4: line[i] = (line[i] + paeth(a, up, c)) & 255
        prev = line
        px = bytearray(stride)
        for i in range(0, stride, 4):
            B, G, R, A = line[i], line[i + 1], line[i + 2], line[i + 3]
            if A and A != 255:
                R = min(255, R * 255 // A); G = min(255, G * 255 // A); B = min(255, B * 255 // A)
            px[i:i + 4] = bytes((R, G, B, A))
        out += b"\0" + px
    png = b"\x89PNG\r\n\x1a\n" + mk(b"IHDR", ihdr) + mk(b"IDAT", zlib.compress(bytes(out), 9)) + mk(b"IEND", b"")
    open(dst, "wb").write(png)
    return "cgbi"


if __name__ == "__main__":
    src_dir, dst_dir = sys.argv[1], sys.argv[2]
    os.makedirs(dst_dir, exist_ok=True)
    n = 0
    for root, _, files in os.walk(src_dir):
        for fn in files:
            if fn.lower().endswith(".png") or fn == "iTunesArtwork":
                rel = os.path.relpath(os.path.join(root, fn), src_dir)
                if "OFResources" in rel or "FBDialog" in rel:
                    continue
                out = os.path.join(dst_dir, os.path.basename(fn) if fn.endswith(".png") else fn + ".png")
                try:
                    convert(os.path.join(root, fn), out); n += 1
                except Exception as e:
                    print("FAIL", rel, e)
    print("converted", n)
