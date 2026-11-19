# Contributing

Thanks for helping! Please read this first.

## Never commit game data
Do not add any file from the original game to the repository, to issues or to pull requests:
* `.ipa` files and `.app` folders
* images, audio, the font, text tables
* screenshots of dump contents
* decompiler output

The build and the tests must keep working without game data. CI uses synthetic files only.
`.gitignore` already excludes the usual locations (`assets/`, `extracted/`, `*.ipa`, …).

## Building
* Windows: `docs/BUILD_WINDOWS.md`
* Linux: `docs/BUILD_LINUX.md`

Development builds compile the env-var test hooks (`ZIGGURAT_TEST_HOOKS`, ON by default) described in `docs/ARCHITECTURE.md`. Release builds turn them off.

## Tests
Run these before opening a PR (CI runs the same):
```
build/ziginputtest
build/zigsteam selftest
build/zigextras selftest
python -I tools/tests/make_fake_ipa.py build/zigimport
python -I tools/tests/stats_roundtrip.py build/zigstats
python -I tools/check_strings.py
python -I tools/check_game_text.py
```
* **Game logic** (`port/src/game/`): keep the original behaviour as the default. If you fix an original bug,
  put the fix behind an enhancement toggle (see `docs/ENHANCEMENTS.md`). Keep the comment with the
  original selector/address above each function.
* **Game text**: never write the game's own text into the source. Refer to it by ID (`txt::k...` in
  `port/src/core/GameText.h`) and look it up with `L()`; the text comes from the player's dump.
  `check_game_text.py` must pass.
* **Launcher text**: add every new string to the string table (`port/src/launcher/Strings.cpp`) and to all
  `port/src/launcher/strings/*.json`. `check_strings.py` must pass.

## Style
* C++17, 4-space indent, braces on the same line, no exceptions in hot paths.
* Match the surrounding code.
* Zero compiler warnings: CI builds with warnings as errors.

## Pull requests
* Keep them focused and describe how you tested.
* Never commit anything from the original game.
* Never touch the user's real Steam files in tests: use the `--steam-root` sandbox options.
