"""Determinism check: the same seeded fixed-step run must be bit-identical under heap fills, heap
perturbations and different directories (docs/TESTING.md).

    python -I tools/tests/determinism.py [--build port/build] [--assets DIR] [--frames 3600] [--every 10]
                                         [variant ...]
variant = base | fill=0xNN | perturb=<bytes> | dir=<name>   (default: a representative matrix)
Exit code 0 when every variant matches the first one (frames and statistics).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import _common as c  # noqa: E402

DEFAULT = ["base", "perturb=333", "perturb=77777", "perturb=40000000", "fill=0x00", "fill=0xFF",
           "dir=deeper", "base"]


def extra(ap):
    ap.add_argument("--frames", type=int, default=3600)
    ap.add_argument("--every", type=int, default=10)
    ap.add_argument("variants", nargs="*")


def main():
    a = c.parse_args("determinism", extra)
    variants = a.variants or DEFAULT
    ref, failed = None, False
    for i, v in enumerate(variants):
        d = os.path.join(a.out, "%02d_%s" % (i, v.replace("=", "_")))
        if v.startswith("dir="):
            d = os.path.join(d, "some", "deeper", "folder_" + v[4:])
        c.make_portable(d, a.build, a.assets)
        env = c.seeded_env(a.frames, a.every, c.taps_default())
        if v.startswith("fill="):
            env["ZIGGURAT_HEAP_FILL"] = v[5:]
        if v.startswith("perturb="):
            env["ZIGGURAT_HEAP_PERTURB"] = v[8:]
        rc = c.run_game(d, env)
        h, st = c.frame_hashes(d), c.stats(d)
        if ref is None:
            ref = (h, st)
            print("[%s] rc=%s frames=%d (reference)" % (v, rc, len(h)))
            continue
        diff = [f for f in ref[0] if h.get(f) != ref[0][f]]
        same = st == ref[1]
        failed |= bool(diff) or not same or rc != 0
        print("[%s] rc=%s frames=%d differ=%d stats_same=%s first=%s" % (v, rc, len(h), len(diff), same, diff[:5]))
    print("DETERMINISM", "FAILED" if failed else "PASSED")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
