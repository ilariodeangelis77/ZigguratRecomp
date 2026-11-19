"""Launcher screenshot + text-overflow sweep (LAUNCHER_PLAN.md L9).

For every language and every tab/page, starts a portable copy of the launcher with ZUI_OVERFLOW_LOG=1,
saves a canvas screenshot and collects 'ZUI-OVERFLOW' log lines. Writes one contact sheet per language.

Usage: python -I tools/ui_sweep.py [--langs en,de,...] [--out _ext/l9test/sweep]
Needs: port/build (ZigguratLauncher.exe, fonts/, strings/), an imported data dir template
       (_ext/verify_launcher/data by default), the sandbox Steam root _ext/l6test/FakeSteam.
"""
import concurrent.futures as cf
import json
import os
import shutil
import struct
import subprocess
import sys
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "port", "build")
# test fixtures (local, not part of the repository): override with ZIGGURAT_SWEEP_DATA / ZIGGURAT_STEAM_SANDBOX
DATA_TEMPLATE = os.environ.get("ZIGGURAT_SWEEP_DATA", os.path.join(ROOT, "_ext", "verify_launcher", "data"))
STEAM_SANDBOX = os.environ.get("ZIGGURAT_STEAM_SANDBOX", os.path.join(ROOT, "_ext", "l6test", "FakeSteam"))
# SDL3.dll: ZIGGURAT_SDL3_DLL, else <MINGW_PREFIX>/bin (set by the MSYS2 shell), else next to the build
SDL_DLL = os.environ.get("ZIGGURAT_SDL3_DLL") or next(
    (p for p in [os.path.join(os.environ.get("MINGW_PREFIX", "/nonexistent"), "bin", "SDL3.dll"),
                 os.path.join(BUILD, "SDL3.dll")] if os.path.isfile(p)),
    os.path.join(BUILD, "SDL3.dll"))
LANGS = ["en", "de", "es", "fr", "it", "pt", "pt-BR"]
# (tab, page, name); page -1 = tab without page selector
SCREENS = [(0, -1, "home"), (1, -1, "display"), (2, -1, "audio")] + \
          [(3, p, "controls%d" % p) for p in range(4)] + [(4, -1, "language")] + \
          [(5, p, "game%d" % p) for p in range(5)] + [(6, p, "data%d" % p) for p in range(4)] + \
          [(7, p, "steam%d" % p) for p in range(2)] + [(8, p, "about%d" % p) for p in range(2)]


def read_bmp(p):
    b = open(p, "rb").read()
    off = struct.unpack_from("<I", b, 10)[0]
    w, h = struct.unpack_from("<ii", b, 18)
    bpp = struct.unpack_from("<H", b, 28)[0] // 8
    st = (w * bpp + 3) & ~3
    rows = []
    for y in range(abs(h)):
        yy = abs(h) - 1 - y if h > 0 else y
        r = b[off + yy * st: off + yy * st + w * bpp]
        rows.append(bytes(c for i in range(0, len(r), bpp) for c in (r[i + 2], r[i + 1], r[i])))
    return w, abs(h), rows


def write_png(path, w, h, rows):
    raw = b"".join(b"\0" + r for r in rows)
    ch = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d))
    open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
                          ch(b"IDAT", zlib.compress(raw, 6)) + ch(b"IEND", b""))


def contact(files, out, cols=4, scale=2):
    tiles = [read_bmp(f) for f in files if os.path.exists(f)]
    if not tiles:
        return
    tw, th = 480 // scale, 320 // scale
    rows_n = (len(tiles) + cols - 1) // cols
    W, H = cols * tw, rows_n * th
    canvas = [bytearray(W * 3) for _ in range(H)]
    for k, (w, h, rows) in enumerate(tiles):
        ox, oy = (k % cols) * tw, (k // cols) * th
        for y in range(th):
            src = rows[y * scale]
            line = canvas[oy + y]
            for x in range(tw):
                i = x * scale * 3
                line[(ox + x) * 3:(ox + x) * 3 + 3] = src[i:i + 3]
    write_png(out, W, H, [bytes(r) for r in canvas])


def prepare(app):
    if os.path.isdir(app):
        shutil.rmtree(app)
    os.makedirs(app)
    for f in ("ZigguratLauncher.exe", "ziggurat.exe"):
        shutil.copy2(os.path.join(BUILD, f), app)
    shutil.copy2(SDL_DLL, app)
    shutil.copytree(os.path.join(BUILD, "fonts"), os.path.join(app, "fonts"))
    shutil.copytree(os.path.join(BUILD, "strings"), os.path.join(app, "strings"))
    shutil.copytree(DATA_TEMPLATE, os.path.join(app, "data"), ignore=shutil.ignore_patterns("logs"))
    open(os.path.join(app, "portable.txt"), "w").close()


def set_lang(app, lang, devopts=True):
    cfgp = os.path.join(app, "data", "config.json")
    cfg = json.load(open(cfgp, encoding="utf-8"))
    cfg.setdefault("game", {})["language"] = lang
    enh = cfg.setdefault("enhancements", {})
    enh["dev_options"] = devopts
    enh["photosensitivity_notice_shown"] = True
    json.dump(cfg, open(cfgp, "w", encoding="utf-8"), indent=2)


def run_lang(lang, outdir):
    app = os.path.join(outdir, "app_" + lang)
    prepare(app)
    set_lang(app, lang)
    shots, overflow = [], []
    env = dict(os.environ, ZUI_OVERFLOW_LOG="1", ZIGGURAT_STEAM_ROOT=STEAM_SANDBOX)
    for tab, page, name in SCREENS:
        bmp = os.path.join(outdir, "%s_%s.bmp" % (lang, name))
        args = [os.path.join(app, "ZigguratLauncher.exe"), "--portable", "--steam-root", STEAM_SANDBOX,
                "--tab", str(tab), "--shots", "1400:" + bmp]
        if page >= 0:
            args += ["--page", str(page)]
        try:
            p = subprocess.run(args, cwd=app, env=env, capture_output=True, text=True, timeout=40,
                               encoding="utf-8", errors="replace")
            log = p.stdout + p.stderr
        except subprocess.TimeoutExpired:
            log = "TIMEOUT"
        for line in log.splitlines():
            if "ZUI-OVERFLOW" in line:
                overflow.append("%s/%s: %s" % (lang, name, line.split("ZUI-OVERFLOW", 1)[1].strip()))
        shots.append(bmp)
    contact(shots, os.path.join(outdir, "sheet_%s.png" % lang))
    return lang, sorted(set(overflow))


def main():
    langs = LANGS
    out = os.path.join(ROOT, "_ext", "l9test", "sweep")
    a = sys.argv[1:]
    while a:
        k = a.pop(0)
        if k == "--langs":
            langs = a.pop(0).split(",")
        elif k == "--out":
            out = a.pop(0)
    os.makedirs(out, exist_ok=True)
    total = 0
    with cf.ThreadPoolExecutor(max_workers=4) as ex:
        for lang, ov in ex.map(lambda l: run_lang(l, out), langs):
            total += len(ov)
            print("== %s: %d overflow(s)" % (lang, len(ov)))
            for o in ov:
                print("   " + o)
    print("TOTAL OVERFLOWS:", total)
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
