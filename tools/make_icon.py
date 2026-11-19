"""Generate the ZiGGURAT port icon: an original pixel-art "Z" in the style of the game's logo
(slanted chunky Z, yellow banded gradient, purple inline, red outline) over a night sky with the
red stepped ziggurat. Drawn procedurally - contains no pixels from the game's assets, so it can be
shipped with the port.

Usage: python -I tools/make_icon.py <outdir>
Writes icon_<N>.png for N in 16 24 32 48 64 128 256, ziggurat.ico (all sizes, PNG-compressed
entries) and icon_preview.png (all sizes side by side on light and dark backgrounds)."""
import os
import struct
import sys
import zlib

# palette (LAUNCHER_PLAN.md §4.1)
SKY_TOP = (0x11, 0x00, 0x1F)
SKY_BOT = (0x37, 0x00, 0x60)
INLINE = (0x26, 0x00, 0x43)
OUTLINE = (0xC5, 0x13, 0x1B)
YEL = [(0xFE, 0xFF, 0x6F), (0xF4, 0xF5, 0x40), (0xDF, 0xE0, 0x12), (0xC4, 0xC5, 0x0C)]
RIVET = (0xD6, 0xD1, 0xDA)
ZIG = (0x87, 0x00, 0x16)
ZIG_HI = (0xFE, 0x00, 0x2A)
ZIG_DK = (0x45, 0x01, 0x0C)
STAR = (0xFF, 0xFF, 0xFF)
MOON = (0xE8, 0xE2, 0xF0)

# Z outline in unit coordinates (x right, y down), then sheared for the italic slant.
Z_POLY = [(0.00, 0.00), (1.00, 0.00), (1.00, 0.22), (0.38, 0.78), (1.00, 0.78), (1.00, 1.00),
          (0.00, 1.00), (0.00, 0.78), (0.62, 0.22), (0.00, 0.22)]
SHEAR = 0.28


def inside(poly, x, y):
    c = False
    n = len(poly)
    for i in range(n):
        x1, y1 = poly[i]
        x2, y2 = poly[(i + 1) % n]
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1:
            c = not c
    return c


def z_mask(n, box):
    """Rasterize the slanted Z into an n x n boolean mask inside box=(x0,y0,x1,y1) in pixels."""
    x0, y0, x1, y1 = box
    w, h = x1 - x0, y1 - y0
    poly = []
    for (u, v) in Z_POLY:
        uu = u * (1 - SHEAR) + (1 - v) * SHEAR  # shear: top shifted right
        poly.append((x0 + uu * w, y0 + v * h))
    m = [[False] * n for _ in range(n)]
    for y in range(n):
        for x in range(n):
            hits = 0
            for sy in (0.25, 0.75):
                for sx in (0.25, 0.75):
                    hits += inside(poly, x + sx, y + sy)
            m[y][x] = hits >= 2
    return m


def dilate(m):
    n = len(m)
    out = [[False] * n for _ in range(n)]
    for y in range(n):
        for x in range(n):
            if m[y][x]:
                out[y][x] = True
                continue
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < n and 0 <= xx < n and m[yy][xx]:
                        out[y][x] = True
    return out


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t + 0.5) for i in range(3))


def render(n):
    img = [[(0, 0, 0, 0)] * n for _ in range(n)]
    detailed = n >= 32
    # background: rounded square night sky
    r = max(2, n // 6)
    for y in range(n):
        for x in range(n):
            # rounded corners
            cx = min(x, n - 1 - x)
            cy = min(y, n - 1 - y)
            if cx < r and cy < r and (r - cx - 0.5) ** 2 + (r - cy - 0.5) ** 2 > r * r:
                continue
            t = y / (n - 1)
            c = lerp(SKY_TOP, SKY_BOT, t)
            if detailed and (x * 7 + y * 13) % 97 == 0 and y < n * 0.55:
                c = STAR
            img[y][x] = c + (255,)
    if detailed:
        # moon (top left: the slanted Z leaves that corner free); too cramped below 48 px
        mr = n * 0.085 if n >= 48 else 0
        mx, my = n * 0.19, n * 0.17
        for y in range(n if mr else 0):
            for x in range(n):
                d = ((x + 0.5 - mx) ** 2 + (y + 0.5 - my) ** 2) ** 0.5
                if d <= mr and img[y][x][3]:
                    img[y][x] = MOON + (255,)
        # stepped ziggurat at the bottom: red stone blocks with dark mortar (like mainZig.png)
        steps = 3 if n < 48 else 4
        base_y = n
        step_h = max(3, int(n * 0.085))
        top_y = base_y - steps * step_h
        brick_w = max(3, n // 12)
        for s in range(steps):
            y0 = base_y - (s + 1) * step_h
            half = (n * 0.50) * (1 - s / (steps + 0.4))
            x0, x1 = int(n / 2 - half), int(n / 2 + half)
            for y in range(y0, y0 + step_h):
                for x in range(max(0, x0), min(n, x1)):
                    if not img[y][x][3]:
                        continue
                    c = ZIG
                    if y == y0:
                        c = ZIG_HI                      # lit top edge of each step
                    elif n >= 32 and y == y0 + step_h - 1:
                        c = ZIG_DK                      # mortar under each course
                    elif n >= 48 and (x - x0 + (s % 2) * (brick_w // 2)) % brick_w == 0:
                        c = ZIG_DK                      # vertical mortar, staggered per course
                    if x == x0 or x == x1 - 1:
                        c = ZIG_DK
                    img[y][x] = c + (255,)
        zbox = (n * 0.17, n * 0.08, n * 0.87, top_y - n * 0.06)
    else:
        zbox = (n * 0.12, n * 0.12, n * 0.88, n * 0.88)
    fill = z_mask(n, zbox)
    inl = dilate(fill)
    out = dilate(inl) if n >= 24 else inl
    ys = [y for y in range(n) if any(fill[y])]
    fy0, fy1 = (min(ys), max(ys)) if ys else (0, 1)
    for y in range(n):
        for x in range(n):
            if not img[y][x][3]:
                continue
            if fill[y][x]:
                t = (y - fy0) / max(1, fy1 - fy0)
                band = min(len(YEL) - 1, int(t * len(YEL)))
                img[y][x] = YEL[band] + (255,)
            elif inl[y][x]:
                img[y][x] = (INLINE if n >= 24 else OUTLINE) + (255,)
            elif out[y][x]:
                img[y][x] = OUTLINE + (255,)
    if n >= 48:
        # rivet highlight on the top-right corner of the Z
        for y in range(n):
            for x in range(n - 1, -1, -1):
                if fill[y][x]:
                    rx, ry = x - 1, y + 1
                    for dy in range(max(1, n // 48)):
                        for dx in range(max(1, n // 48)):
                            img[ry + dy][rx - dx] = RIVET + (255,)
                    break
            else:
                continue
            break
    return img


def scale(img, k):
    return [[px for px in row for _ in range(k)] for row in img for _ in range(k)]


def png_bytes(img):
    h, w = len(img), len(img[0])
    raw = b"".join(b"\0" + bytes(c for px in row for c in px) for row in img)
    ch = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
            + ch(b"IDAT", zlib.compress(raw, 9)) + ch(b"IEND", b""))


def main(outdir):
    os.makedirs(outdir, exist_ok=True)
    # native renders for small sizes; 128/256 are the 64 render scaled x2/x4 (chunky pixel look)
    imgs = {}
    for n in (16, 24, 32, 48, 64):
        imgs[n] = render(n)
    imgs[128] = scale(imgs[64], 2)
    imgs[256] = scale(imgs[64], 4)
    pngs = {}
    for n, im in imgs.items():
        pngs[n] = png_bytes(im)
        open(os.path.join(outdir, "icon_%d.png" % n), "wb").write(pngs[n])
    # .ico with PNG-compressed entries (Vista+)
    sizes = sorted(pngs)
    header = struct.pack("<HHH", 0, 1, len(sizes))
    entries, data = b"", b""
    offset = 6 + 16 * len(sizes)
    for n in sizes:
        d = pngs[n]
        entries += struct.pack("<BBBBHHII", n % 256, n % 256, 0, 0, 1, 32, len(d), offset)
        data += d
        offset += len(d)
    open(os.path.join(outdir, "ziggurat.ico"), "wb").write(header + entries + data)
    # C array of the 64 px PNG for SDL_SetWindowIcon (window icon on every platform)
    with open(os.path.join(outdir, "icon_png.inc"), "w") as f:
        f.write("// generated by tools/make_icon.py - original artwork, not from the game's assets\n")
        d = pngs[64]
        f.write("static const unsigned char kIconPng[%d] = {\n" % len(d))
        for i in range(0, len(d), 20):
            f.write("    " + ", ".join("0x%02x" % b for b in d[i:i + 20]) + ",\n")
        f.write("};\n")
    # preview: every size at x2 on dark and light strips
    pad = 8
    W = sum(n * 2 + pad for n in sizes) + pad
    H = (256 * 2 // 2 + pad * 2) * 2
    prev = [[(40, 40, 48, 255) if y < H // 2 else (235, 235, 240, 255) for _ in range(W)] for y in range(H)]
    for half in (0, 1):
        x = pad
        for n in sizes:
            k = 2 if n <= 128 else 1
            im = scale(imgs[n], k)
            oy = half * (H // 2) + pad
            for y, row in enumerate(im):
                for xx, px in enumerate(row):
                    if px[3]:
                        prev[oy + y][x + xx] = px
            x += len(im[0]) + pad
    open(os.path.join(outdir, "icon_preview.png"), "wb").write(png_bytes(prev))
    print("wrote", outdir)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "port/res")
