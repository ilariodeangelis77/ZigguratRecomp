"""Post-process Ghidra output: annotate CFString addresses and float bit-patterns.
Usage: python -I tools/annotate_decomp.py <Zig2_armv7 thin binary> <decomp_dir>
 - 0x25xxxx constants inside __cfstring  ->  0x250844 /*@"idle"*/
 - stores of 0x3e99999a-style ints        ->  0x3e99999a /*0.3f*/
"""
import os, re, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import macho  # noqa: E402


def main(binpath, root):
    data = open(binpath, "rb").read()
    s = macho.Slice(data, 0, len(data), "armv7")
    a, size, fo = s.sects[("__DATA", "__cfstring")]
    cf = {}
    for off in range(0, size, 16):
        isa, flags, ptr, length = struct.unpack_from("<4I", s.d, fo + off)
        o = s.v2o(ptr)
        if o is None:
            continue
        raw = s.d[o:o + (length * 2 if flags & 0x4 == 0 and False else length)]
        try:
            txt = raw.decode("utf-8")
        except UnicodeDecodeError:
            txt = raw.decode("latin1")
        cf[a + off] = txt.replace("*/", "*_/").replace("\n", "\\n")
    hexre = re.compile(r"0x([0-9a-f]{5,8})\b")

    def repl(m):
        v = int(m.group(1), 16)
        if v in cf:
            return '%s /*@"%s"*/' % (m.group(0), cf[v])
        if len(m.group(1)) == 8 and 0x30000000 <= v <= 0x4f000000:
            f = struct.unpack("<f", struct.pack("<I", v))[0]
            return "%s /*%gf*/" % (m.group(0), f)
        return m.group(0)

    n = 0
    for dp, _, files in os.walk(root):
        for fn in files:
            if fn.endswith(".c"):
                p = os.path.join(dp, fn)
                t = open(p, encoding="utf-8").read()
                t2 = hexre.sub(repl, t)
                if t2 != t:
                    open(p, "w", encoding="utf-8").write(t2)
                    n += 1
    print("cfstrings:", len(cf), "files annotated:", n)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
