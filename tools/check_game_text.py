"""Keep the game's own text out of this project (see port/src/core/GameText.h).

The game's on-screen text is read at runtime from the player's dump and referenced in code only by
ID (32-bit FNV-1a hash of the original English lookup key). This check:
  1. reads the IDs from port/src/core/GameText.h;
  2. hashes every double-quoted string in port/src (code and comments) and every string value in
     the launcher translations (port/src/launcher/strings/*.json), and fails if one matches an ID,
     i.e. if the game's text itself was written into the project. IDs marked "generic" (short UI
     words the launcher also uses) only fail inside port/src/game/;
  3. with --dump <assets dir>, also checks that every ID resolves to an entry of the imported
     <assets dir>/strings/en.json (needs your own imported game files; not run in CI).

Usage: python -I tools/check_game_text.py [--dump <assets dir>]
Exit 0 = ok, 1 = problems.
"""
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "port", "src")
HEADER = os.path.join(SRC, "core", "GameText.h")
ID_RE = re.compile(r"constexpr Id (\w+) = 0x([0-9a-fA-F]{8});(\s*// generic)?")
STR_RE = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
ESCAPES = {"n": "\n", "t": "\t", "\\": "\\", '"': '"', "'": "'", "0": "\0"}
EXTS = (".cpp", ".h", ".hpp", ".c", ".inc", ".rc")


def fnv1a(s):
    h = 2166136261
    for b in s.encode("utf-8"):
        h ^= b
        h = (h * 16777619) & 0xFFFFFFFF
    return h


def unescape(s):
    return re.sub(r"\\(.)", lambda m: ESCAPES.get(m.group(1), m.group(0)), s)


def load_ids():
    ids = {}
    with open(HEADER, encoding="utf-8") as f:
        for m in ID_RE.finditer(f.read()):
            ids[int(m.group(2), 16)] = (m.group(1), bool(m.group(3)))
    return ids


def main():
    args = sys.argv[1:]
    dump = None
    if "--dump" in args:
        i = args.index("--dump")
        if i + 1 >= len(args):
            sys.exit("--dump needs a directory")
        dump = args[i + 1]

    ids = load_ids()
    if not ids:
        print("no IDs found in " + os.path.relpath(HEADER, ROOT))
        return 1
    problems = []

    def check(rel, line, s):
        hit = ids.get(fnv1a(s))
        if not hit:
            return
        name, generic = hit
        if generic and not rel.startswith("game/"):
            return
        problems.append("%s:%d: game text written out (use txt::%s)" % (rel, line, name))

    for path in sorted(glob.glob(os.path.join(SRC, "**", "*"), recursive=True)):
        rel = os.path.relpath(path, SRC).replace(os.sep, "/")
        if path.endswith(EXTS):
            with open(path, encoding="utf-8", errors="replace") as f:
                for n, text in enumerate(f, 1):
                    for m in STR_RE.finditer(text):
                        check(rel, n, unescape(m.group(1)))
        elif path.endswith(".json") and rel.startswith("launcher/strings/"):
            with open(path, encoding="utf-8") as f:
                lines = f.read().split("\n")
            for n, text in enumerate(lines, 1):
                for m in STR_RE.finditer(text):
                    check(rel, n, json.loads('"%s"' % m.group(1)))

    resolved = None
    if dump:
        en = os.path.join(dump, "strings", "en.json")
        with open(en, encoding="utf-8") as f:
            keys = {fnv1a(k) for k in json.load(f)}
        missing = [name for h, (name, _) in ids.items() if h not in keys]
        for name in sorted(missing):
            problems.append("txt::%s does not match any entry of %s" % (name, en))
        resolved = len(ids) - len(missing)

    for p in problems:
        print(p)
    summary = "%d text IDs, %d problem(s)" % (len(ids), len(problems))
    if resolved is not None:
        summary += ", %d/%d resolved in the dump" % (resolved, len(ids))
    print(summary)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
