"""Build the distributable zip (LAUNCHER_PLAN.md L9).

    python -I tools/package.py [--selftest <ZiGGURAT.ipa>]

Creates dist/ZigguratRecomp-<version>-windows-x64.zip containing ONLY our own executables, the SDL3 /
winpthreads runtime DLLs, the OFL fallback font, launcher strings, licences and a README.
It never contains game assets, the IPA, Flixel.ttf or touchHLE: the listing is audited and the
script fails if anything that looks like game data slips in.

The executables come from a RELEASE build (port/build_release, configured with
-DZIGGURAT_TEST_HOOKS=OFF so no test hook or developer screen is compiled in). Options:
    --no-build          package the existing port/build_release without rebuilding
    --mingw <prefix>    MinGW prefix holding bin/SDL3.dll (default: $MINGW_PREFIX, else autodetected
                        from `where SDL3.dll` / `where gcc`); not needed when SDL3 is built from source
                        (ZIGGURAT_FETCH_SDL3=ON puts SDL3.dll next to the executables)

--selftest: unzips into a fresh temp folder with portable mode and a PATH of only Windows system
folders, imports the dump with the dev build's zigimport (the release launcher has no test flags),
boots the release game and checks its log, starts the release launcher briefly, and proves the
release executables ignore test hooks (ZIGGURAT_DUMP_EVERY writes no frames).
"""
import glob
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = os.path.join(ROOT, "port")
BUILD = os.path.join(PORT, "build_release")
DEV_BUILD = os.path.join(PORT, "build")
DLLS = ["SDL3.dll", "libwinpthread-1.dll"]


def arg(name):
    return sys.argv[sys.argv.index(name) + 1] if name in sys.argv and sys.argv.index(name) + 1 < len(sys.argv) else None


def find_mingw():
    """MinGW prefix (folder with bin/ and share/licenses/): --mingw, $MINGW_PREFIX, or autodetect."""
    cand = arg("--mingw") or os.environ.get("MINGW_PREFIX")
    if cand:
        return os.path.normpath(cand)
    for tool in ("SDL3.dll", "gcc.exe", "gcc"):
        try:
            out = subprocess.run(["where", tool], capture_output=True, text=True).stdout.splitlines()
        except OSError:
            out = []
        for line in out:
            if line.strip():
                return os.path.dirname(os.path.dirname(line.strip()))
    raise SystemExit("MinGW not found: pass --mingw <prefix> or set MINGW_PREFIX")


MINGW = None  # resolved in main


def release_build():
    """Configure + build port/build_release with test hooks compiled out."""
    env = dict(os.environ)
    if MINGW:
        env["PATH"] = os.path.join(MINGW, "bin") + os.pathsep + env.get("PATH", "")
    # SDL3 is built from the pinned source (ZIGGURAT_FETCH_SDL3), not taken from MSYS2: the MSYS2
    # package changes under us (it started linking libiconv-2.dll, which 0.9.0 then lacked).
    cfg = ["cmake", "-S", PORT, "-B", BUILD, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
           "-DZIGGURAT_TEST_HOOKS=OFF", "-DZIGGURAT_FETCH_SDL3=ON"]
    if MINGW:
        cfg.append("-DCMAKE_PREFIX_PATH=" + MINGW)
    subprocess.run(cfg, check=True, env=env, stdout=subprocess.DEVNULL)
    r = subprocess.run(["cmake", "--build", BUILD, "--target", "SDL3-shared", "ZigguratLauncher", "ziggurat"], env=env,
                       capture_output=True, text=True)
    # real diagnostics only ("file:line: warning: ..."), not file names such as SDL_error.c
    noise = [l for l in r.stdout.splitlines() if re.search(r"\b(warning|error)\b\s*:", l, re.I)]
    if r.returncode != 0 or noise:
        print(r.stdout[-4000:])
        raise SystemExit("release build failed or produced warnings")
    print("release build ok (ZIGGURAT_TEST_HOOKS=OFF):", BUILD)


def runtime_file(name):
    """A runtime DLL: next to the release executables (SDL3 built from source) or in MinGW's bin/."""
    local = os.path.join(BUILD, name)
    if os.path.exists(local):
        return local
    if MINGW and os.path.exists(os.path.join(MINGW, "bin", name)):
        return os.path.join(MINGW, "bin", name)
    raise SystemExit("runtime file not found: " + name)


def pe_imports(path):
    """Names of the DLLs a PE file imports (import directory only; enough for MinGW output)."""
    import struct
    b = open(path, "rb").read()
    pe = struct.unpack_from("<I", b, 0x3C)[0]
    if b[pe:pe + 4] != b"PE\0\0":
        raise SystemExit("not a PE file: " + path)
    nsec, = struct.unpack_from("<H", b, pe + 6)
    optsz, = struct.unpack_from("<H", b, pe + 20)
    opt = pe + 24
    magic, = struct.unpack_from("<H", b, opt)
    dd = opt + (112 if magic == 0x20B else 96)
    imp_rva, = struct.unpack_from("<I", b, dd + 8)
    secs = [struct.unpack_from("<IIII", b, opt + optsz + i * 40 + 8) for i in range(nsec)]

    def off(rva):  # (vsize, vaddr, rawsize, rawptr)
        for vs, va, rs, rp in secs:
            if va <= rva < va + max(vs, rs):
                return rva - va + rp
        raise SystemExit("bad RVA in " + path)
    out, o = [], off(imp_rva) if imp_rva else None
    while o is not None:
        name_rva = struct.unpack_from("<I", b, o + 12)[0]
        if not name_rva:
            break
        n = off(name_rva)
        out.append(b[n:b.index(b"\0", n)].decode("ascii"))
        o += 20
    return out


def audit_dlls(stage):
    """Every DLL any packaged binary imports must be in the zip or be part of Windows itself."""
    sysdir = os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32")
    have = {f.lower() for f in os.listdir(stage)}
    missing = {}
    for f in sorted(os.listdir(stage)):
        if not f.lower().endswith((".exe", ".dll")):
            continue
        for d in pe_imports(os.path.join(stage, f)):
            dl = d.lower()
            if dl in have or dl.startswith(("api-ms-win-", "ext-ms-")) or os.path.exists(os.path.join(sysdir, d)):
                continue
            missing.setdefault(d, []).append(f)
    if missing:
        raise SystemExit("MISSING DLLS (not in the zip, not part of Windows): %s" % missing)
    print("audit: every imported DLL is in the zip or part of Windows")


def sdl3_license():
    for p in [os.path.join(BUILD, "_deps", "sdl3-src", "LICENSE.txt"),
              os.path.join(MINGW or "", "share", "licenses", "SDL3", "LICENSE.txt")]:
        if os.path.exists(p):
            return p
    raise SystemExit("SDL3 LICENSE.txt not found")
EXES = ["ZigguratLauncher.exe", "ziggurat.exe"]

README = """ZiGGURAT Recomp {version} - unofficial fan port (Windows x64)
==========================================================

This is an UNOFFICIAL, non-commercial fan reconstruction of ZiGGURAT (2012, iOS) by
Action Button Entertainment, for preservation. It is not affiliated with Action Button
Entertainment or Freshuu.

NO GAME FILES ARE INCLUDED. You need your own copy of the original iOS game (an .ipa file
or the Zig2.app folder). The launcher reads the art, music and text from it on first start.

How to use
----------
1. Unzip this folder anywhere (it does not need installing).
2. Start ZigguratLauncher.exe.
3. Choose your ZiGGURAT .ipa (or the folder containing it, or drag it onto the window).
   The files are checked against known hashes and imported (about 14 MB).
4. Press PLAY NOW. Settings (display, audio, controllers, language, accessibility) are in the tabs.
   The launcher can also add the game to your Steam library with artwork made from your files.

Data location
-------------
Settings, records, logs and the imported files are stored in %APPDATA%\\ZigguratRecomp\\.
PORTABLE MODE: create an empty file named portable.txt next to ZigguratLauncher.exe to keep
everything in a "data" folder next to the program instead.

Files
-----
ZigguratLauncher.exe   the launcher (setup, settings, Add to Steam)
ziggurat.exe           the game (can also be started directly once the game files are imported)
SDL3.dll, libwinpthread-1.dll   runtime libraries
fonts\\                 Silkscreen (SIL Open Font License), used before your game files are imported
strings\\               launcher translations (drafts; corrections welcome)
LICENSES\\              licences of the open-source components

Credits
-------
Original game: Tim Rogers (design/director), Michael Kerwin (program), Brent Porter (graphics),
Andrew Toups (sound). Engine: flixel-ios by Semi Secret Software (MIT).
"""

FORBIDDEN = re.compile(r"\.(png|jpg|jpeg|wav|mp3|ipa|plist|strings|mom|omo)$|flixel\.ttf$|touchhle|zig2\.app|"
                       r"itunesartwork|highscore", re.I)


def version():
    # A release tag (CI: GITHUB_REF_NAME=v0.9.0-rc1) names the zip, matching the Linux archive.
    tag = os.environ.get("GITHUB_REF_NAME", "")
    if tag.startswith("v") and os.environ.get("GITHUB_REF_TYPE", "tag") == "tag":
        return tag[1:]
    s = open(os.path.join(PORT, "src", "core", "Version.h"), encoding="utf-8").read()
    return re.search(r'kVersion\s*=\s*"([^"]+)"', s).group(1)


def block(path, start, end=None):
    s = open(path, encoding="utf-8", errors="replace").read()
    i = s.find(start)
    if i < 0:
        raise SystemExit("licence text not found in " + path)
    j = s.find(end, i) if end else -1
    return s[i:j if j > 0 else None].strip() + "\n"


def licences(dst):
    os.makedirs(dst, exist_ok=True)
    tp = os.path.join(PORT, "third_party")
    shutil.copy2(os.path.join(PORT, "src", "engine", "ENGINE_LICENSE.txt"), os.path.join(dst, "flixel-ios-MIT.txt"))
    shutil.copy2(sdl3_license(), os.path.join(dst, "SDL3-zlib.txt"))
    wp = os.path.join(MINGW or "", "share", "licenses", "winpthreads", "COPYING")
    if os.path.exists(wp):
        shutil.copy2(wp, os.path.join(dst, "winpthreads.txt"))
    shutil.copy2(os.path.join(tp, "fonts", "OFL.txt"), os.path.join(dst, "Silkscreen-OFL.txt"))
    open(os.path.join(dst, "stb.txt"), "w", encoding="utf-8").write(
        "stb_image, stb_image_write, stb_truetype (Sean Barrett)\n\n" +
        block(os.path.join(tp, "stb_image.h"), "This software is available under 2 licenses", "*/"))
    open(os.path.join(dst, "dr_libs.txt"), "w", encoding="utf-8").write(
        "dr_wav, dr_mp3 (David Reid)\n\n" +
        block(os.path.join(tp, "dr_wav.h"), "This software is available as a choice of the following licenses", "*/"))
    hdr = open(os.path.join(tp, "json.hpp"), encoding="utf-8").read(2000)
    cr = re.search(r"SPDX-FileCopyrightText: (.+)", hdr).group(1).strip()
    open(os.path.join(dst, "nlohmann-json-MIT.txt"), "w", encoding="utf-8").write(
        "JSON for Modern C++ (nlohmann/json)\nCopyright (c) " + cr + "\n\n" + MIT)
    open(os.path.join(dst, "sqlite.txt"), "w", encoding="utf-8").write(
        "SQLite (amalgamation, port/third_party/sqlite)\n\n" +
        block(os.path.join(tp, "sqlite", "sqlite3.c"), "The author disclaims copyright", "***"))


MIT = """Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the
following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial
portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO
EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
OR OTHER DEALINGS IN THE SOFTWARE.
"""


def build():
    ver = version()
    name = "ZigguratRecomp-%s-windows-x64" % ver
    dist = os.path.join(ROOT, "dist")
    stage = os.path.join(dist, name)
    if os.path.isdir(stage):
        shutil.rmtree(stage)
    os.makedirs(stage)
    for e in EXES:
        shutil.copy2(os.path.join(BUILD, e), stage)
    for d in DLLS:
        if d == "libwinpthread-1.dll" and not MINGW:
            continue  # only MinGW builds need it
        shutil.copy2(runtime_file(d), stage)
    os.makedirs(os.path.join(stage, "fonts"))
    for f in ("Silkscreen-Regular.ttf", "OFL.txt"):
        shutil.copy2(os.path.join(PORT, "third_party", "fonts", f), os.path.join(stage, "fonts"))
    os.makedirs(os.path.join(stage, "strings"))
    for f in glob.glob(os.path.join(PORT, "src", "launcher", "strings", "*.json")):
        json.load(open(f, encoding="utf-8"))  # must parse
        shutil.copy2(f, os.path.join(stage, "strings"))
    audit_dlls(stage)
    licences(os.path.join(stage, "LICENSES"))
    open(os.path.join(stage, "README.txt"), "w", encoding="utf-8", newline="\r\n").write(README.format(version=ver))
    zpath = os.path.join(dist, name + ".zip")
    if os.path.exists(zpath):
        os.remove(zpath)
    with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for dp, _, files in os.walk(stage):
            for f in sorted(files):
                full = os.path.join(dp, f)
                z.write(full, os.path.join(name, os.path.relpath(full, stage)))
    # audit
    with zipfile.ZipFile(zpath) as z:
        names = z.namelist()
    bad = [n for n in names if FORBIDDEN.search(n)]
    print("zip:", zpath, "%.1f MB" % (os.path.getsize(zpath) / 1e6), len(names), "files")
    for n in names:
        print("  ", n)
    if bad:
        raise SystemExit("FORBIDDEN CONTENT IN ZIP: %s" % bad)
    print("audit: no game assets / IPA / touchHLE in the zip")
    return zpath, name


def _stop_from(folder):
    # stop only processes started from THIS temp folder (other instances are left alone)
    ps = ("Get-Process | Where-Object { $_.Path -and $_.Path.StartsWith('%s', 'OrdinalIgnoreCase') } | "
          "Stop-Process -Force" % folder.replace("'", "''"))
    subprocess.run(["powershell", "-NoProfile", "-Command", ps], capture_output=True)


def selftest(zpath, name, ipa):
    tmp = tempfile.mkdtemp(prefix="zigpkg_")
    with zipfile.ZipFile(zpath) as z:
        z.extractall(tmp)
    app = os.path.join(tmp, name)
    open(os.path.join(app, "portable.txt"), "w").close()
    data = os.path.join(app, "data")
    assets = os.path.join(data, "assets")
    env = dict(os.environ)
    # clean PATH: only Windows system dirs, so missing DLLs would really fail
    env["PATH"] = os.pathsep.join(p for p in env["PATH"].split(os.pathsep) if p.lower().startswith(r"c:\windows"))
    t0 = time.time()
    results = {}

    # 1) import with the DEV build's zigimport (release launcher has no test flags)
    zi = os.path.join(DEV_BUILD, "zigimport.exe")
    r = subprocess.run([zi, "import", ipa, assets], capture_output=True, text=True)
    n_assets = len(os.listdir(assets)) if os.path.isdir(assets) else 0
    results["import"] = r.returncode == 0 and n_assets > 150
    cfg_path = os.path.join(data, "config.json")
    json.dump({"game": {"dump_source": ipa, "dump_verified": True, "dump_version": "0001.00.01"}},
              open(cfg_path, "w", encoding="utf-8"))

    # 2) boot the release game (--shot exits after frame 60) and check its log
    with open(os.path.join(tmp, "game_stdout.txt"), "w") as out:
        try:
            p = subprocess.run([os.path.join(app, "ziggurat.exe"), "--portable", "--shot", "60",
                                os.path.join(tmp, "final.bmp")], cwd=app, env=env, stdout=out,
                               stderr=subprocess.STDOUT, timeout=60)
            game_rc = p.returncode
        except subprocess.TimeoutExpired:
            game_rc = "timeout"
    logs = sorted(glob.glob(os.path.join(data, "logs", "game-*.log")))
    glog = open(logs[-1], encoding="utf-8", errors="replace").read() if logs else ""
    results["game"] = game_rc == 0 and "portable=1" in glog and "assets=" in glog and os.path.exists(os.path.join(tmp, "final.bmp"))

    # 3) test hooks must be inert in release executables: ZIGGURAT_DUMP_EVERY would write frames/
    henv = dict(env, ZIGGURAT_DUMP_EVERY="1", ZIGGURAT_MAX_FRAMES="30", ZIGGURAT_FIXED_DT="1")
    with open(os.path.join(tmp, "hooks_stdout.txt"), "w") as out:
        try:
            subprocess.run([os.path.join(app, "ziggurat.exe"), "--portable", "--shot", "30",
                            os.path.join(tmp, "hooks.bmp")], cwd=app, env=henv, stdout=out,
                           stderr=subprocess.STDOUT, timeout=60)
        except subprocess.TimeoutExpired:
            pass
    results["hooks_off"] = not os.path.isdir(os.path.join(app, "frames"))

    # 4) start the release launcher briefly: it must come up with the imported theme and log no errors
    lp = subprocess.Popen([os.path.join(app, "ZigguratLauncher.exe"), "--portable"], cwd=app, env=env,
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(6)
    alive = lp.poll() is None
    if lp.poll() is None:  # always stop the launcher we started (path-based cleanup can miss it)
        lp.kill()
        try:
            lp.wait(10)
        except subprocess.TimeoutExpired:
            pass
    _stop_from(app)
    llogs = sorted(glob.glob(os.path.join(data, "logs", "launcher-*.log")))
    llog = open(llogs[-1], encoding="utf-8", errors="replace").read() if llogs else ""
    results["launcher"] = alive and bool(llogs) and "[ERROR]" not in llog

    _stop_from(app)
    ok = all(results.values())
    print("selftest dir:", tmp)
    print("  results:", results, " imported assets:", n_assets, " (%.1fs)" % (time.time() - t0))
    for line in glog.splitlines()[:4]:
        print("  game log:", line)
    print("SELFTEST", "PASSED" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    if "--no-build" not in sys.argv or arg("--mingw") or os.environ.get("MINGW_PREFIX"):
        MINGW = find_mingw()
    else:
        try:
            MINGW = find_mingw()
        except SystemExit:
            MINGW = None
    if "--no-build" not in sys.argv:
        release_build()
    zpath, name = build()
    if "--selftest" in sys.argv:
        sys.exit(selftest(zpath, name, arg("--selftest")))
