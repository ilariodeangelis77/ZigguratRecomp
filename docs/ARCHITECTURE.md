# Architecture

```
port/
  src/engine/    flixel-ios (Semi Secret, MIT) translated to C++: FlxGame, FlxState, FlxSprite, FlxGroup,
                 FlxEmitter, FlxText, FlxU, SemiSecretTexture/Font/Text, ... (original names kept)
  src/platform/  SDL3 layer: window + display modes, GLES1-style quad emulation via SDL_Renderer,
                 OpenAL-style sound sources, AVAudioPlayer-style music, touch/mouse/gamepad virtual finger
  src/game/      reconstruction of the ZiGGURAT game and menu logic (see src/game/README.md)
  src/core/      "zigcore", shared by the game, the launcher and the CLIs: Config (config.json), Paths
                 (data dir, portable mode), Log, DumpImport (scan/verify/import, CgBI PNG conversion,
                 plist parsing, 188-file manifest), StatsStore, InputMap, Steam (binary VDF, shortcut IDs,
                 backups), ArtworkGen, Shortcuts, SteamInput, UpdateCheck, AppIcon
  src/launcher/  the launcher: ZigUI (480x320 pixel UI toolkit themed from the user's own assets),
                 App + screens (wizard, tabs), strings/ (7 languages)
  src/tools/     test and maintenance CLIs (zigimport, zigstats, zigsteam, zigextras, ziginputtest)
  third_party/   single-header libraries, SQLite amalgamation, fallback OFL font
  res/           the project's own icon (tools/make_icon.py) and Windows resources
tools/           Python helpers: asset extraction, manifest generation, packaging, string checks,
                 UI sweeps, icon generation, Mach-O/ObjC metadata parsing; tools/ghidra/ scripts
```

## Runtime flow
1. The **launcher** finds a verified dump through the wizard and imports it into `<data>/assets/`, converting Apple's CgBI PNGs to standard PNGs and string tables to JSON.
2. The launcher writes `config.json` and starts `ziggurat`.
3. The game reads the config, applies display, audio, input and enhancements, then runs the original state flow: logos → title → (tutorial) → play → game over.
4. A non-zero exit code shows the crash panel. Exit code 2 (missing files) sends the user back to the wizard.

## Fidelity rules for `src/game/`
* Same class and method names as the original, with the Objective-C selector and address in a comment above each function.
* Same order of operations and the same constants; `float` vs `double` promotion kept.
* The original bugs are kept. Fixes live behind `enh()` flags (see `docs/ENHANCEMENTS.md`).

## How the reconstruction was made (high level)
* The original Mach-O binary's Objective-C metadata (class, method and ivar names) was read with `tools/macho.py`.
* Methods were decompiled with Ghidra 12, using the `__stdcall_softfp` calling convention (`tools/ghidra/SetSoftfp.java`; iOS armv7 passes floats in core registers).
* The output was annotated with CFString literals and float constants (`tools/annotate_decomp.py`) and rewritten by hand into C++.
* Behaviour was compared frame by frame against the original running in an emulator, with scripted input.
* No decompiler output or game data is part of this repository.

## Test hooks (CMake option `ZIGGURAT_TEST_HOOKS`, ON by default, OFF in release builds)
Environment variables used by CI and local verification:
* `ZIGGURAT_TAPS` / `ZIGGURAT_PAD`: scripted touch and gamepad input
* `ZIGGURAT_FIXED_DT`: fixed timestep, unpaced
* `ZIGGURAT_SEED`: deterministic random streams
* `ZIGGURAT_DUMP_EVERY` / `ZIGGURAT_MAX_FRAMES`: frame dumps
* `ZIGGURAT_WINDOW_SHOT`: final window capture
* `ZIGGURAT_STEAM_ROOT`: Steam sandbox
* plus a few more, documented next to their code

With a fixed seed and timestep, runs are bit-identical across memory layouts and install folders.
`ZIGGURAT_LANG` (force a language) is a normal user feature and always available.
