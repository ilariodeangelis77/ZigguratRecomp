"""Shared helpers for the headless test drivers in tools/tests/ (docs/TESTING.md).

Paths are resolved from arguments / environment, never hard-coded:
  --build <dir>    dev build with test hooks ON (default: port/build)
  --assets <dir>   imported game files (default: $ZIGGURAT_TEST_ASSETS, else port/assets)
  --out <dir>      where runs are written (default: <build>/test-runs/<script name>)
Runtime DLLs (SDL3.dll, libwinpthread-1.dll) are taken from next to the exe, from $MINGW_PREFIX/bin,
or from `where SDL3.dll`.
The game files are YOUR imported dump: they are never committed or shipped.
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DLLS = ("SDL3.dll", "libwinpthread-1.dll")


def parse_args(script, extra=None):
    ap = argparse.ArgumentParser(description=script)
    ap.add_argument("--build", default=os.path.join(ROOT, "port", "build"))
    ap.add_argument("--assets", default=os.environ.get("ZIGGURAT_TEST_ASSETS", os.path.join(ROOT, "port", "assets")))
    ap.add_argument("--out", default=None)
    if extra:
        extra(ap)
    a = ap.parse_args()
    a.build = os.path.abspath(a.build)
    a.assets = os.path.abspath(a.assets)
    a.out = os.path.abspath(a.out or os.path.join(a.build, "test-runs", script))
    if not os.path.isfile(os.path.join(a.assets, "ziglogo.png")):
        raise SystemExit("game files not found in %s (pass --assets or set ZIGGURAT_TEST_ASSETS)" % a.assets)
    return a


def runtime_dll(build, name):
    local = os.path.join(build, name)
    if os.path.isfile(local):
        return local
    pre = os.environ.get("MINGW_PREFIX")
    if pre and os.path.isfile(os.path.join(pre, "bin", name)):
        return os.path.join(pre, "bin", name)
    try:
        out = subprocess.run(["where", name], capture_output=True, text=True).stdout.splitlines()
        if out and out[0].strip():
            return out[0].strip()
    except OSError:
        pass
    return None


def make_portable(dest, build, assets, config=None, defaults="firstStartup\tno\n"):
    """Fresh portable copy of the game in dest: exe + DLLs + portable.txt + data/config.json."""
    if os.path.exists(dest):
        shutil.rmtree(dest)
    os.makedirs(os.path.join(dest, "data"))
    shutil.copy(os.path.join(build, "ziggurat.exe"), dest)
    for n in DLLS:
        p = runtime_dll(build, n)
        if p:
            shutil.copy(p, dest)
    open(os.path.join(dest, "portable.txt"), "w").close()
    cfg = dict(config or {})
    cfg.setdefault("game", {})
    cfg["game"] = dict(cfg["game"], assets_dir=assets.replace("\\", "/"))
    json.dump(cfg, open(os.path.join(dest, "data", "config.json"), "w"), indent=1)
    if defaults is not None:
        open(os.path.join(dest, "data", "defaults.tsv"), "w").write(defaults)
    os.makedirs(os.path.join(dest, "frames"))
    return dest


def run_game(dest, env_extra, args=(), timeout=1200):
    e = dict(os.environ)
    for k, v in env_extra.items():
        if v == "":
            e.pop(k, None)   # "" = unset
        else:
            e[k] = v
    with open(os.path.join(dest, "run.log"), "w") as log:
        return subprocess.call([os.path.join(dest, "ziggurat.exe"), "--portable", *args], cwd=dest, env=e,
                               stdout=log, stderr=subprocess.STDOUT, timeout=timeout)


def frame_hashes(dest):
    fr = sorted(os.listdir(os.path.join(dest, "frames")))
    return {f: hashlib.md5(open(os.path.join(dest, "frames", f), "rb").read()).hexdigest() for f in fr}


def framediff(a, b):
    ha, hb = frame_hashes(a), frame_hashes(b)
    return [f for f in ha if hb.get(f) != ha[f]], len(ha)


def stats(dest):
    out = {}
    sp = os.path.join(dest, "data", "statistics.txt")
    if os.path.exists(sp):
        for line in open(sp):
            k, _, v = line.strip().partition("=")
            if k:
                out[k] = float(v)
    return out


def taps_default():
    """Title tap, 41 charged shots, retry on the game-over screen, 21 more shots."""
    t = ["12000:240:200:200"]
    t += ["%d:%d:280:700" % (15000 + i * 1500, 160 + (i % 5) * 40) for i in range(41)]
    t += ["80000:390:215:200"]
    t += ["%d:%d:280:700" % (83000 + i * 1500, 160 + (i % 5) * 40) for i in range(21)]
    return ";".join(t)


def taps_long(seconds):
    t = ["12000:240:200:200"]
    t += ["%d:%d:280:700" % (15000 + i * 1300, 120 + (i % 7) * 40) for i in range(int(seconds / 1.3))]
    return ";".join(t)


def seeded_env(frames, every, taps):
    return {"ZIGGURAT_SEED": "4242", "ZIGGURAT_FIXED_DT": "1", "ZIGGURAT_DUMP_EVERY": str(every),
            "ZIGGURAT_MAX_FRAMES": str(frames), "ZIGGURAT_TAPS": taps}
