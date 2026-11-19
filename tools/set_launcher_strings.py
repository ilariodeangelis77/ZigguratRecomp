"""Set launcher strings: English entries in port/src/launcher/Strings.cpp and/or a translation JSON.

Usage: python -I tools/set_launcher_strings.py <updates.json>
updates.json: {"en": {"id": "text", ...}, "it": {...}, ...}
English is patched inside the C++ table ({"id", "text"} entries); other languages in strings/<lang>.json.
Afterwards regenerate en.json with: ZigguratLauncher --write-strings port/src/launcher/strings/en.json
"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LDIR = os.path.join(ROOT, "port", "src", "launcher")


def cpp_escape(s):
    return s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")


def patch_cpp(src, key, text):
    tag = '{"%s", "' % key
    i = src.find(tag)
    if i < 0:
        raise SystemExit("id not found in Strings.cpp: " + key)
    j = i + len(tag)
    k = j
    while True:  # find the closing quote, honouring backslash escapes
        if src[k] == "\\":
            k += 2
            continue
        if src[k] == '"':
            break
        k += 1
    return src[:j] + cpp_escape(text) + src[k:]


def main():
    upd = json.load(open(sys.argv[1], encoding="utf-8"))
    for lang, entries in upd.items():
        if lang == "en":
            p = os.path.join(LDIR, "Strings.cpp")
            s = open(p, encoding="utf-8").read()
            for k, v in entries.items():
                s = patch_cpp(s, k, v)
            open(p, "w", encoding="utf-8", newline="\n").write(s)
        else:
            p = os.path.join(LDIR, "strings", lang + ".json")
            d = json.load(open(p, encoding="utf-8"))
            for k, v in entries.items():
                if k not in d:
                    raise SystemExit("%s: unknown id %s" % (lang, k))
                d[k] = v
            with open(p, "w", encoding="utf-8", newline="\n") as f:
                json.dump(d, f, ensure_ascii=False, indent=2)
                f.write("\n")
        print("%s: %d updated" % (lang, len(entries)))


if __name__ == "__main__":
    main()
