## What does this change?

## How was it tested?
- [ ] `ziginputtest`, `zigsteam selftest`, `zigextras selftest`
- [ ] `tools/tests/make_fake_ipa.py`, `tools/tests/stats_roundtrip.py`, `tools/check_strings.py`
- [ ] Played / checked in the game or launcher (describe)

## Checklist
- [ ] No game files (assets, `.ipa`, decompiler output, screenshots of dump contents) are included
- [ ] Original gameplay behaviour is unchanged by default (fixes are behind enhancement toggles)
- [ ] New launcher strings are in `Strings.cpp` and every `strings/*.json`
- [ ] Steam-related tests used a sandbox (`--steam-root`), never a real Steam install
