"""Synthetic dump tests for zigimport (no game content: tiny placeholder files only).

Creates fake IPAs / wrappers in a temp dir and checks the status zigimport reports:
  NotZiggurat  - valid IPA layout, different bundle id
  Incomplete   - ZiGGURAT bundle id, but almost every required file is missing
  Compressed   - the IPA wrapped inside a .zip
  NotFound     - path that does not exist

Usage: python -I tools/tests/make_fake_ipa.py <path-to-zigimport(.exe)>
Exit code 0 when every case reports the expected status.
"""
import os
import subprocess
import sys
import tempfile
import zipfile

PLIST = """<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleIdentifier</key><string>{bid}</string>
<key>CFBundleShortVersionString</key><string>0001.00.01</string>
<key>CFBundleDisplayName</key><string>{name}</string>
<key>CFBundleExecutable</key><string>Zig2</string>
</dict></plist>
"""


def make_ipa(path, bundle_id, name, extra=None):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("Payload/Zig2.app/Info.plist", PLIST.format(bid=bundle_id, name=name))
        z.writestr("Payload/Zig2.app/Zig2", b"\0" * 64)  # placeholder, not a real binary
        for n, data in (extra or {}).items():
            z.writestr("Payload/Zig2.app/" + n, data)


def status_of(tool, path):
    r = subprocess.run([tool, "verify", path], capture_output=True, text=True)
    for line in (r.stdout + r.stderr).splitlines():
        if line.startswith("status:"):
            return line.split(":", 1)[1].strip()
    return "?(exit %d) %s" % (r.returncode, (r.stdout + r.stderr).strip()[:200])


def main():
    tool = os.path.abspath(sys.argv[1])
    tmp = tempfile.mkdtemp(prefix="zig_fakeipa_")
    other = os.path.join(tmp, "other.ipa")
    make_ipa(other, "com.example.othergame", "Other")
    incomplete = os.path.join(tmp, "incomplete.ipa")
    make_ipa(incomplete, "com.freshuu.ziggurat", "ZiGGURAT", {"bounce.wav": b"RIFF0000WAVE"})
    wrapper = os.path.join(tmp, "wrapped.zip")
    with zipfile.ZipFile(wrapper, "w") as z:
        z.write(incomplete, "ZiGGURAT.ipa")
    cases = [
        ("not ziggurat", other, "NotZiggurat"),
        ("incomplete", incomplete, "Incomplete"),
        ("zip wrapper", wrapper, "Compressed"),
        ("missing path", os.path.join(tmp, "nope.ipa"), "NotFound"),
    ]
    failed = 0
    for label, path, want in cases:
        got = status_of(tool, path)
        ok = got == want
        failed += not ok
        print("%-13s expected %-12s got %-12s %s" % (label, want, got, "ok" if ok else "FAIL"))
    print("fake-ipa tests: %d passed, %d failed" % (len(cases) - failed, failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
