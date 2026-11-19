"""Read 32-bit values at virtual addresses of the armv7 slice. Usage: python -I tools/readdat.py 4d45c 4d460 ..."""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import macho
d = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "re", "Zig2_armv7"), "rb").read()
s = macho.Slice(d, 0, len(d), "armv7")
for a in sys.argv[1:]:
    va = int(a, 16); o = s.v2o(va)
    v = struct.unpack_from("<I", s.d, o)[0]
    dv = struct.unpack_from("<d", s.d, o)[0]
    print(f"{a}: 0x{v:08x} int={struct.unpack('<i',struct.pack('<I',v))[0]} f={struct.unpack('<f',struct.pack('<I',v))[0]:g} d={dv:g}")
