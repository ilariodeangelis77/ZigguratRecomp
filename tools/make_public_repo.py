"""Assemble the public GitHub tree from an explicit allowlist, then audit it.

Usage: python -I tools/make_public_repo.py [target_dir] [--no-git]
  target_dir defaults to ../ZigguratRecomp-public (next to this project folder).

* Idempotent: everything in the target except .git/ is deleted and rebuilt.
* Only ALLOWLIST paths are copied; nothing else in this folder (dumps, decompilation, test data,
  development notes) can reach the public tree.
* DENY scan: fails on game data, oversized files, private paths and personal-data patterns.
* Git: initialises the repo if needed and stages everything. Creates the initial commit only if
  `git config user.email` is a GitHub noreply address (no co-author trailers). Never pushes, never adds remotes.
"""
import codecs
import fnmatch
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

ALLOW = [
    "port/src/**", "port/CMakeLists.txt", "port/res/**", "port/third_party/**",
    "tools/*.py", "tools/ghidra/*.java", "tools/tests/**",
    "docs/**", ".github/**",
    "README.md", "LICENSE", "THIRD_PARTY_LICENSES.md", "CONTRIBUTING.md", "SECURITY.md", "CHANGELOG.md",
    ".gitignore", ".gitattributes", ".clang-format",
]
# private even though they match the allowlist
EXCLUDE = ["tools/__pycache__/**", "**/__pycache__/**", "**/*.pyc", "port/res/icon_preview.png"]

DENY_EXT = {".ipa", ".wav", ".mp3", ".bmp", ".ppm", ".log", ".patch", ".exe", ".dll", ".so"}
DENY_NAMES = {"info.plist", "itunesmetadata.plist", "flixel.ttf", "itunesartwork", "itunesartwork.jpg"}
DENY_DIRS = ["_ext/", "re/", "analysis/", "extracted/", "assets/", "dist/", "patches/", "overrides/"]
ALLOWED_PNG = re.compile(r"^port/res/icon_\d+\.png$")
BIG_OK = {"port/third_party/sqlite/sqlite3.c"}
MAX_BYTES = 2 * 1024 * 1024
# personal-data / provenance patterns, ROT13-encoded so this file does not match itself
_PATTERNS_ROT13 = [r"pnzvyyn", r"vynevb", r"@tznvy", r"[Q]:[\\/]", r"cebtenzzv", r"cebtrggvinev",
                   r"nhqvb_ehfg", r"urnirav4a", r"penpxrq"]
_WHOLE_WORD = {4}  # pattern #5 (an Italian folder name) also occurs inside "programming": match whole words only
PATTERNS = [re.compile((r"\b%s\b" if i in _WHOLE_WORD else "%s") % codecs.decode(p, "rot13"), re.I)
            for i, p in enumerate(_PATTERNS_ROT13)]
# reviewed false positives: (path glob, pattern number) -> reason
EXEMPT = [
    ("port/third_party/**", 3, "upstream author e-mail addresses in licence headers"),
    ("port/third_party/**", 4, "drive-letter examples in upstream comments"),
    ("port/src/launcher/Gallery.cpp", 4, "fake example paths shown in the UI gallery"),
]
TEXT_EXT = {".cpp", ".h", ".hpp", ".c", ".inc", ".py", ".java", ".md", ".txt", ".json", ".yml", ".yaml",
            ".rc", ".manifest", ".cmake", ".gitignore", ".gitattributes", ".clang-format", ""}


def rel(p):
    return os.path.relpath(p, ROOT).replace("\\", "/")


def allowed(r):
    if any(fnmatch.fnmatch(r, e) for e in EXCLUDE):
        return False
    for a in ALLOW:
        if a.endswith("/**"):
            if r.startswith(a[:-2]):
                return True
        elif fnmatch.fnmatch(r, a) and "/" not in r[len(os.path.dirname(a)) + 1 if os.path.dirname(a) else 0:]:
            return True
    return False


def collect():
    files = []
    for dp, dns, fns in os.walk(ROOT):
        r = rel(dp)
        if r.split("/")[0] in {"_ext", "re", "analysis", "extracted", "dist", "patches", ".git"}:
            dns[:] = []
            continue
        dns[:] = [d for d in dns if not d.startswith("build") and d != "__pycache__"]
        for fn in fns:
            fr = rel(os.path.join(dp, fn))
            if allowed(fr):
                files.append(fr)
    return sorted(files)


def deny_scan(target, files):
    problems = []
    for r in files:
        low = r.lower()
        base = os.path.basename(low)
        ext = os.path.splitext(low)[1]
        if ext in DENY_EXT or base in DENY_NAMES:
            problems.append("forbidden file type: " + r)
        if ext == ".png" and not ALLOWED_PNG.match(r):
            problems.append("png outside port/res/icon_*.png: " + r)
        if any(("/" + d) in ("/" + low) for d in DENY_DIRS):
            problems.append("private directory: " + r)
        full = os.path.join(target, r)
        size = os.path.getsize(full)
        if size > MAX_BYTES and r not in BIG_OK:
            problems.append("file > 2 MB: %s (%d bytes)" % (r, size))
        if ext in TEXT_EXT or base in {".gitignore", ".gitattributes"}:
            try:
                text = open(full, encoding="utf-8", errors="replace").read()
            except OSError:
                continue
            for pat in PATTERNS:
                m = pat.search(text)
                n = PATTERNS.index(pat) + 1
                if m and any(fnmatch.fnmatch(r, g) and n == k for g, k, _ in EXEMPT):
                    continue
                if m:
                    line = text.count("\n", 0, m.start()) + 1
                    problems.append("private pattern #%d in %s:%d" % (PATTERNS.index(pat) + 1, r, line))
    return problems


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    target = os.path.abspath(args[0] if args else os.path.join(ROOT, "..", "ZigguratRecomp-public"))
    if os.path.abspath(target).startswith(ROOT + os.sep):
        sys.exit("target must be outside the project folder")
    os.makedirs(target, exist_ok=True)
    for name in os.listdir(target):          # idempotent: wipe everything but .git
        if name == ".git":
            continue
        p = os.path.join(target, name)
        shutil.rmtree(p) if os.path.isdir(p) else os.remove(p)
    files = collect()
    for r in files:
        dst = os.path.join(target, r)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(os.path.join(ROOT, r), dst)
    print("copied %d files to %s" % (len(files), target))
    problems = deny_scan(target, files)
    if problems:
        print("DENY SCAN FAILED (%d):" % len(problems))
        for p in problems:
            print("  " + p)
        return 1
    print("deny scan: ok")
    if "--no-git" in sys.argv:
        return 0
    git = lambda *a, **k: subprocess.run(["git", "-C", target, *a], capture_output=True, text=True, **k)
    if not os.path.isdir(os.path.join(target, ".git")):
        git("init", "-b", "main")
    git("add", "-A")
    email = git("config", "user.email").stdout.strip()
    staged = git("diff", "--cached", "--name-only").stdout.split()
    print("staged changes: %d files" % len(staged))
    if not email.lower().endswith("@users.noreply.github.com"):
        print("NOT COMMITTED: git user.email is not a GitHub noreply address. Set it in the public repo with\n"
              "  git -C <target> config user.email <id>+<user>@users.noreply.github.com\n"
              "and commit yourself (or re-run this script).")
        return 0
    if not staged:
        print("nothing to commit")
        return 0
    has_head = git("rev-parse", "--verify", "HEAD").returncode == 0
    msg = "Update public tree" if has_head else "Initial public release 0.9.0"
    r = git("commit", "-q", "-m", msg)
    print("committed:" if r.returncode == 0 else "commit failed:", git("log", "-1", "--format=%h %s").stdout.strip()
          if r.returncode == 0 else r.stderr.strip())
    return r.returncode


if __name__ == "__main__":
    sys.exit(main())
