"""Validate launcher translations against strings/en.json (LAUNCHER_PLAN.md L9).

Checks, for every <lang>.json next to en.json:
  * every English id is present (missing ids fall back to English at runtime -> reported as errors)
  * no unknown ids (typos)
  * the {0} {1} ... placeholder set is identical to English
  * '\\n' line-break count is identical (the ABOUT staff block relies on it)
  * length: warns when a translation is > 1.6x the English length (+8 chars) — likely to overflow;
    the authoritative overflow check is tools/ui_sweep.py
Keys starting with '_' (e.g. "_meta") are metadata and ignored.

Usage: python -I tools/check_strings.py [dir]   (default: port/src/launcher/strings)
Exit 0 = no errors (warnings allowed), 1 = errors.
"""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PH = re.compile(r"\{\d+\}")


def load(p):
    with open(p, encoding="utf-8") as f:
        d = json.load(f)
    return {k: v for k, v in d.items() if not k.startswith("_")}


def main():
    d = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "port", "src", "launcher", "strings")
    en = load(os.path.join(d, "en.json"))
    errors = warnings = 0
    langs = sorted(f[:-5] for f in os.listdir(d) if f.endswith(".json") and f != "en.json")
    if not langs:
        print("no translations found")
        return 1
    for lang in langs:
        tr = load(os.path.join(d, lang + ".json"))
        e0, w0 = errors, warnings
        for k in sorted(set(en) - set(tr)):
            print("%s: MISSING %s" % (lang, k))
            errors += 1
        for k in sorted(set(tr) - set(en)):
            print("%s: UNKNOWN id %s" % (lang, k))
            errors += 1
        for k in sorted(set(en) & set(tr)):
            a, b = en[k], tr[k]
            if not isinstance(b, str) or not b.strip():
                print("%s: EMPTY %s" % (lang, k))
                errors += 1
                continue
            if sorted(PH.findall(a)) != sorted(PH.findall(b)):
                print("%s: PLACEHOLDERS %s: en %s vs %s" % (lang, k, PH.findall(a), PH.findall(b)))
                errors += 1
            if a.count("\n") != b.count("\n"):
                print("%s: LINEBREAKS %s" % (lang, k))
                errors += 1
            if len(b) > len(a) * 1.6 + 8:
                print("%s: long %s (%d vs %d chars)" % (lang, k, len(b), len(a)))
                warnings += 1
        print("== %s: %d strings, %d error(s), %d warning(s)" % (lang, len(tr), errors - e0, warnings - w0))
    print("TOTAL: %d language(s), %d error(s), %d warning(s)" % (len(langs), errors, warnings))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
