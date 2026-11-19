import struct, sys, collections

CPU = {12: "ARM"}
SUB = {6: "armv6", 9: "armv7", 11: "armv7s"}
LC_NAMES = {
    0x1: "SEGMENT", 0x2: "SYMTAB", 0xb: "DYSYMTAB", 0xc: "LOAD_DYLIB", 0xe: "LOAD_DYLINKER",
    0x5: "UNIXTHREAD", 0x21: "ENCRYPTION_INFO", 0x1d: "CODE_SIGNATURE", 0x22: "DYLD_INFO",
    0x80000022: "DYLD_INFO_ONLY", 0x1b: "UUID", 0x24: "VERSION_MIN_IPHONEOS", 0x26: "FUNCTION_STARTS",
    0x29: "DATA_IN_CODE", 0x2a: "SOURCE_VERSION", 0x80000028: "MAIN", 0x80000018: "LOAD_WEAK_DYLIB",
}


def cstr(b, off):
    e = b.index(b"\0", off)
    return b[off:e].decode("latin1")


class Slice:
    def __init__(self, data, base, size, name):
        self.d = data[base:base + size]
        self.name = name
        magic, cpu, sub, ftype, ncmds, sizeofcmds, flags = struct.unpack_from("<7I", self.d, 0)
        assert magic == 0xFEEDFACE, hex(magic)
        self.segs = []
        self.sects = {}
        self.dylibs = []
        self.lcs = []
        self.enc = None
        self.symtab = None
        off = 28
        for _ in range(ncmds):
            cmd, sz = struct.unpack_from("<2I", self.d, off)
            self.lcs.append(LC_NAMES.get(cmd, hex(cmd)))
            if cmd == 0x1:
                segname = self.d[off + 8:off + 24].rstrip(b"\0").decode()
                vmaddr, vmsize, fileoff, filesize, maxp, initp, nsects, fl = struct.unpack_from("<8I", self.d, off + 24)
                self.segs.append((segname, vmaddr, vmsize, fileoff, filesize))
                so = off + 56
                for i in range(nsects):
                    sn = self.d[so:so + 16].rstrip(b"\0").decode()
                    sg = self.d[so + 16:so + 32].rstrip(b"\0").decode()
                    addr, ssz, sfo = struct.unpack_from("<3I", self.d, so + 32)
                    self.sects[(sg, sn)] = (addr, ssz, sfo)
                    so += 68
            elif cmd in (0xc, 0x80000018):
                noff = struct.unpack_from("<I", self.d, off + 8)[0]
                self.dylibs.append(cstr(self.d, off + noff) + (" (weak)" if cmd != 0xc else ""))
            elif cmd == 0x21:
                self.enc = struct.unpack_from("<3I", self.d, off + 8)
            elif cmd == 0x2:
                self.symtab = struct.unpack_from("<4I", self.d, off + 8)
            off += sz

    def v2o(self, va):
        for name, vmaddr, vmsize, fileoff, filesize in self.segs:
            if vmaddr <= va < vmaddr + filesize:
                return fileoff + (va - vmaddr)
        return None

    def u32(self, va):
        o = self.v2o(va)
        return None if o is None else struct.unpack_from("<I", self.d, o)[0]

    def strva(self, va):
        o = self.v2o(va)
        return None if o is None else cstr(self.d, o)

    def objc_classes(self):
        if ("__DATA", "__objc_classlist") not in self.sects:
            return []
        addr, size, fo = self.sects[("__DATA", "__objc_classlist")]
        out = []
        for i in range(size // 4):
            cls = struct.unpack_from("<I", self.d, fo + i * 4)[0]
            out.append(self.read_class(cls))
        return out

    def read_methods(self, ml):
        if not ml:
            return []
        o = self.v2o(ml)
        entsize, count = struct.unpack_from("<2I", self.d, o)
        entsize &= ~3
        res = []
        for j in range(count):
            nm, ty, imp = struct.unpack_from("<3I", self.d, o + 8 + j * entsize)
            res.append((self.strva(nm), self.strva(ty), imp))
        return res

    def read_ivars(self, il):
        if not il:
            return []
        o = self.v2o(il)
        entsize, count = struct.unpack_from("<2I", self.d, o)
        res = []
        for j in range(count):
            offp, nm, ty, al, sz = struct.unpack_from("<5I", self.d, o + 8 + j * entsize)
            res.append((self.strva(nm), self.strva(ty)))
        return res

    def read_class(self, cls):
        isa, superc, cache, vt, ro = struct.unpack_from("<5I", self.d, self.v2o(cls))
        ro &= ~3
        flags, istart, isize, ivlayout, name, bm, bp, ivars, wl, props = struct.unpack_from("<10I", self.d, self.v2o(ro))
        meta_ro = self.u32(isa + 16) & ~3
        mbm = struct.unpack_from("<I", self.d, self.v2o(meta_ro) + 20)[0]
        sname = None
        if superc:
            sro = self.u32(superc + 16)
            if sro is not None:
                sname = self.strva(struct.unpack_from("<I", self.d, self.v2o(sro & ~3) + 16)[0])
        return dict(name=self.strva(name), super=sname, size=isize,
                    imethods=self.read_methods(bm), cmethods=self.read_methods(mbm), ivars=self.read_ivars(ivars))

    def undefined_symbols(self):
        symoff, nsyms, stroff, strsize = self.symtab
        res = []
        for i in range(nsyms):
            strx, ntype, nsect, ndesc, nval = struct.unpack_from("<IBBhI", self.d, symoff + i * 12)
            if (ntype & 0x0e) == 0 and (ntype & 0x01):
                res.append(cstr(self.d, stroff + strx))
        return res

    def all_symbols(self):
        symoff, nsyms, stroff, strsize = self.symtab
        res = []
        for i in range(nsyms):
            strx, ntype, nsect, ndesc, nval = struct.unpack_from("<IBBhI", self.d, symoff + i * 12)
            res.append((cstr(self.d, stroff + strx), ntype, nval))
        return res


def load(path):
    data = open(path, "rb").read()
    magic = struct.unpack_from(">I", data, 0)[0]
    slices = []
    if magic == 0xCAFEBABE:
        n = struct.unpack_from(">I", data, 4)[0]
        for i in range(n):
            cpu, sub, off, size, align = struct.unpack_from(">5I", data, 8 + i * 20)
            slices.append(Slice(data, off, size, SUB.get(sub, str(sub))))
    else:
        slices.append(Slice(data, 0, len(data), "thin"))
    return slices


if __name__ == "__main__":
    path, mode = sys.argv[1], sys.argv[2]
    sl = load(path)
    if mode == "info":
        for s in sl:
            print("==", s.name, "size", len(s.d))
            print(" load cmds:", s.lcs)
            print(" encryption (cryptoff,cryptsize,cryptid):", s.enc)
            for seg in s.segs:
                print(" seg", seg[0], hex(seg[1]), "vmsize", hex(seg[2]), "filesize", hex(seg[4]))
            for k, v in s.sects.items():
                print("   sect", k, hex(v[0]), "size", v[1])
            print(" dylibs:")
            for d in s.dylibs:
                print("   ", d)
            print(" nsyms:", s.symtab)
    elif mode == "classes":
        s = [x for x in sl if x.name == sys.argv[3]][0]
        for c in s.objc_classes():
            print(f"@interface {c['name']} : {c['super']}   // instance size {c['size']}")
            for n, t in c["ivars"]:
                print(f"    ivar {n} {t}")
            for n, t, imp in c["cmethods"]:
                print(f"  + {n}   types={t}   imp={hex(imp)}")
            for n, t, imp in c["imethods"]:
                print(f"  - {n}   types={t}   imp={hex(imp)}")
    elif mode == "undef":
        s = [x for x in sl if x.name == sys.argv[3]][0]
        for u in sorted(set(s.undefined_symbols())):
            print(u)
    elif mode == "syms":
        s = [x for x in sl if x.name == sys.argv[3]][0]
        for n, t, v in s.all_symbols():
            print(hex(t), hex(v), n)
