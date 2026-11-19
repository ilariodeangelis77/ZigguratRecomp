"""Build the port's asset folder from the user's own ZiGGURAT IPA.
Usage: python -I tools/extract_assets.py <ZiGGURAT.ipa> <out_assets_dir>
- PNG (Apple CgBI) -> standard PNG
- WAV / MP3 / TTF copied
- *.lproj/Localizable.strings (binary plist) -> strings/<lang>.json
"""
import io, json, os, plistlib, sys, zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cgbi2png  # noqa: E402

SKIP = ("OFResources_Universal.bundle", "FBDialog.bundle", "_CodeSignature")


def main(ipa, out):
    os.makedirs(os.path.join(out, "strings"), exist_ok=True)
    n = {"png": 0, "audio": 0, "font": 0, "strings": 0}
    with zipfile.ZipFile(ipa) as z:
        for info in z.infolist():
            name = info.filename
            if not name.startswith("Payload/Zig2.app/") or name.endswith("/"):
                continue
            rel = name[len("Payload/Zig2.app/"):]
            if any(s in rel for s in SKIP):
                continue
            base = os.path.basename(rel)
            data = z.read(info)
            low = base.lower()
            if low.endswith(".png"):
                tmp = os.path.join(out, base + ".cgbi")
                open(tmp, "wb").write(data)
                cgbi2png.convert(tmp, os.path.join(out, base))
                os.remove(tmp)
                n["png"] += 1
            elif low.endswith((".wav", ".mp3")):
                open(os.path.join(out, base), "wb").write(data); n["audio"] += 1
            elif low.endswith(".ttf"):
                open(os.path.join(out, base), "wb").write(data); n["font"] += 1
            elif base == "Localizable.strings":
                lang = rel.split("/")[0].replace(".lproj", "")
                d = plistlib.load(io.BytesIO(data))
                json.dump(d, open(os.path.join(out, "strings", lang + ".json"), "w", encoding="utf-8"),
                          ensure_ascii=False, indent=1)
                n["strings"] += 1
    print("extracted", n)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
