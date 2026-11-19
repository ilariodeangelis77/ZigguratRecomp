"""Asset override folder check (docs/TESTING.md): an EMPTY overrides folder must not change a seeded
run, and an override file must be picked up.

    python -I tools/tests/overrides_cases.py [--build port/build] [--assets DIR]
Exit code 0 when: off vs empty-overrides frames are identical (beyond same-config noise, which must be 0
since runs are deterministic) and the tinted sky override changes frames and is logged.
"""
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402

TAPS = ";".join(["12000:240:200:200"] + ["%d:%d:280:700" % (15000 + i * 1500, 160 + (i % 5) * 40) for i in range(16)])


def tinted_png(src):
    """Decode a standard 8-bit RGBA/RGB PNG (filter types 0-4) and return it tinted green."""
    b = open(src, "rb").read()
    o, idat = 8, b""
    while o < len(b):
        ln = struct.unpack(">I", b[o:o + 4])[0]
        t = b[o + 4:o + 8]
        dd = b[o + 8:o + 8 + ln]
        if t == b"IHDR":
            w, h, bd, ct = struct.unpack(">IIBB", dd[:10])
        if t == b"IDAT":
            idat += dd
        o += 12 + ln
    raw = zlib.decompress(idat)
    bpp = 4 if ct == 6 else 3
    st = w * bpp
    prev = bytearray(st)
    out = bytearray()
    i = 0
    for _ in range(h):
        f = raw[i]
        i += 1
        line = bytearray(raw[i:i + st])
        i += st
        for x in range(st):
            a = line[x - bpp] if x >= bpp else 0
            cc = prev[x - bpp] if x >= bpp else 0
            u = prev[x]
            if f == 1:
                line[x] = (line[x] + a) & 255
            elif f == 2:
                line[x] = (line[x] + u) & 255
            elif f == 3:
                line[x] = (line[x] + ((a + u) >> 1)) & 255
            elif f == 4:
                p = a + u - cc
                pa, pb, pc = abs(p - a), abs(p - u), abs(p - cc)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else u if pb <= pc else cc)) & 255
        prev = line
        row = bytearray(line)
        for x in range(0, st, bpp):
            row[x] //= 3
            row[x + 2] //= 3
        out += b"\0" + row

    def ch(t, d_):
        return struct.pack(">I", len(d_)) + t + d_ + struct.pack(">I", zlib.crc32(t + d_))
    return (b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, ct, 0, 0, 0))
            + ch(b"IDAT", zlib.compress(bytes(out))) + ch(b"IEND", b""))


def main():
    a = c.parse_args("overrides_cases")

    def run(name, overrides, files=()):
        d = os.path.join(a.out, name)
        c.make_portable(d, a.build, a.assets, {"extras": {"overrides": overrides}})
        os.makedirs(os.path.join(d, "data", "overrides"), exist_ok=True)
        for rel, data in files:
            p = os.path.join(d, "data", "overrides", rel)
            os.makedirs(os.path.dirname(p), exist_ok=True)
            open(p, "wb").write(data)
        rc = c.run_game(d, c.seeded_env(1800, 60, TAPS), timeout=600)
        return d, rc

    off_a, r1 = run("off_a", False)
    off_b, r2 = run("off_b", False)
    empty, r3 = run("on_empty", True)
    sky, r4 = run("on_sky", True, [("sky_day.png", tinted_png(os.path.join(a.assets, "sky_day.png")))])
    noise, n = c.framediff(off_a, off_b)
    d_empty, _ = c.framediff(off_a, empty)
    d_sky, _ = c.framediff(off_a, sky)
    logged = any("overrides:" in l for l in open(os.path.join(sky, "run.log"), errors="replace"))
    print("rc:", r1, r2, r3, r4, " frames per run:", n)
    print("off vs off (noise): %d differ; off vs empty overrides: %d differ; off vs sky override: %d differ; "
          "override logged: %s" % (len(noise), len(d_empty), len(d_sky), logged))
    ok = not noise and not d_empty and d_sky and logged and r1 == r2 == r3 == r4 == 0
    print("OVERRIDES", "PASSED" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
