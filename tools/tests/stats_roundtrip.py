"""Records round-trip test for zigstats (synthetic numbers, portable temp data dir).

  JSON -> make-test-sqlite (CoreData-shaped HighScore.sqlite) -> import-sqlite -> export -> compare
  then export -> import (JSON) -> export again -> compare.

Usage: python -I tools/tests/stats_roundtrip.py <path-to-zigstats(.exe)>
zigstats is copied into a temp dir and run with --portable, so the real data folder is never touched.
Any shared libraries next to the tool (SDL3.dll, libSDL3.so*) are copied along.
"""
import glob
import json
import os
import shutil
import subprocess
import sys
import tempfile

VALUES = {"gamesPlayed": 8, "kills": 41, "headShots": 18, "shots": 90, "bestKills": 14,
          "bestSeconds": 31.25, "averageKills": 5.125, "blueKills": 33, "ufoHits": 2}


def run(tool, *args):
    r = subprocess.run([tool, "--portable", *args], capture_output=True, text=True)
    if r.returncode != 0:
        print("command failed:", args, r.stdout, r.stderr)
        sys.exit(1)
    return r.stdout


def exported(tool, path):
    run(tool, "export", path)
    with open(path, encoding="utf-8") as f:
        return json.load(f)["statistics"]


def main():
    src = os.path.abspath(sys.argv[1])
    tmp = tempfile.mkdtemp(prefix="zig_stats_")
    tool = os.path.join(tmp, os.path.basename(src))
    shutil.copy2(src, tool)
    for lib in glob.glob(os.path.join(os.path.dirname(src), "*SDL3*")):
        if os.path.isfile(lib):
            shutil.copy2(lib, tmp)
    seed = os.path.join(tmp, "seed.json")
    with open(seed, "w", encoding="utf-8") as f:
        json.dump({"format": "ziggurat-stats", "version": 1, "statistics": VALUES}, f)
    db = os.path.join(tmp, "HighScore.sqlite")
    subprocess.run([tool, "make-test-sqlite", db, seed], check=True, capture_output=True)
    run(tool, "import-sqlite", db)
    a = exported(tool, os.path.join(tmp, "a.json"))
    bad = [k for k, v in VALUES.items() if abs(a.get(k, -1) - v) > 1e-6]
    run(tool, "import", os.path.join(tmp, "a.json"))
    b = exported(tool, os.path.join(tmp, "b.json"))
    if a != b:
        bad.append("json round-trip differs")
    # keep-best merge: bestKills/bestSeconds/bestShots/bestMissedShots are one round (as in the
    # original's updateStats), so the whole group comes from the side with more kills.
    group = ("bestKills", "bestSeconds", "bestShots", "bestMissedShots")
    rnd_a = dict(zip(group, (14, 31.25, 60, 9)))
    rnd_b = dict(zip(group, (20, 40.5, 30, 2)))
    for base, other, want in ((rnd_a, rnd_b, rnd_b), (rnd_b, rnd_a, rnd_b)):
        for name, vals in (("m1.json", base), ("m2.json", other)):
            with open(os.path.join(tmp, name), "w", encoding="utf-8") as f:
                json.dump({"format": "ziggurat-stats", "version": 1, "statistics": vals}, f)
        run(tool, "import", os.path.join(tmp, "m1.json"))
        run(tool, "--keep-best", "import", os.path.join(tmp, "m2.json"))
        m = exported(tool, os.path.join(tmp, "m.json"))
        if any(abs(m.get(k, -1) - want[k]) > 1e-6 for k in group):
            bad.append("keep-best split the best round: %s" % {k: m.get(k) for k in group})
    print("stats round-trip:", "ok" if not bad else "FAIL " + ", ".join(bad))
    shutil.rmtree(tmp, ignore_errors=True)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
