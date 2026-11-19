# Testing

## Build configurations
| Configuration | How | Test hooks |
|---|---|---|
| Development / CI tests | `cmake -S port -B port/build` (default) | **on** (`ZIGGURAT_TEST_HOOKS=ON`) |
| Release packages | `python -I tools/package.py` (configures `port/build_release` with `-DZIGGURAT_TEST_HOOKS=OFF`) | **compiled out** |

All test hooks go through `zig::testEnv()` (`port/src/core/TestHooks.h`). With `ZIGGURAT_TEST_HOOKS=OFF` it always returns `nullptr`, so setting these environment variables has **no effect** on release executables. The launcher's developer flags and screens are also left out of release builds:
* flags: `--gallery`, `--steam-root`, `--wizard-dump`, `--shots`, `--page`, `--fallback`, `--write-strings`, `--glyph-test`, `--width-test`
* the game's `--engine-test`

`tools/package.py --selftest` checks this on the packaged zip: `ZIGGURAT_DUMP_EVERY` must not write any frames.

Two environment variables are **user features**, not test hooks, and work in every build:

| Variable | Effect |
|---|---|
| `ZIGGURAT_LANG` | Force a language (`en`, `de`, `es`, `fr`, `it`, `pt`, `pt-BR`); the launcher sets it from config. |
| `ZIGGURAT_SEED` | Fixed random seed. Used by the launcher's developer option "fixed seed", and by the determinism tests. |

## Test hooks (development builds only)

### Game: deterministic, scripted runs
| Variable | Effect |
|---|---|
| `ZIGGURAT_FIXED_DT` | Fixed 1/30 s step, no frame pacing (runs as fast as possible). Sound "playing" state uses a virtual game-time clock. |
| `ZIGGURAT_TAPS` | Scripted touches: `ms:x:y[:holdms];…` in 480×320 game coordinates. Real mouse/touch input is ignored while active. |
| `ZIGGURAT_TAPS_FRAME_MS` | With `ZIGGURAT_FIXED_DT`: milliseconds per frame for the tap clock (default 33.3). |
| `ZIGGURAT_PAD` | Scripted virtual gamepad: `ms:button:down\|up;ms:axis:leftx:value;…` (fed into `InputMap`). |
| `ZIGGURAT_SYNTH_CLICK` | Push a synthetic mouse click into the event queue (window-coordinate input mapping tests). |
| `ZIGGURAT_DUMP_EVERY` | Save every Nth 480×320 frame to `frames/frame_NNNNN.bmp`. |
| `ZIGGURAT_MAX_FRAMES` | Exit after N frames. |
| `ZIGGURAT_WINDOW_SHOT` / `ZIGGURAT_WINDOW_SHOT_FRAME` | Save the final composited window image (after scaling/borders) at a given frame. |
| `ZIGGURAT_GODMODE` | Start every game in god mode (stats still recorded, unlike the practice option). |
| `ZIGGURAT_KILL_AT` | Kill the player at N seconds of game time (stat bookkeeping tests). |
| `ZIGGURAT_TOUCHLOG` | Log the touch state every frame. |
| `ZIGGURAT_FPSLOG` | Log the measured FPS. |
| `ZIGGURAT_HEAP_FILL` | Fill every `new` allocation with a byte (`0x00`, `0xCD`, `0xFF`…) to expose uninitialised reads. |
| `ZIGGURAT_HEAP_PERTURB` | Allocate and hold N bytes at startup (shifts heap layout). |
| `ZIGGURAT_HEAP_POISON` | Fill freed blocks with `0xDD` and never reuse them (exposes use-after-free). |

### Launcher
| Variable | Effect |
|---|---|
| `ZIGGURAT_UI_SCRIPT` | Scripted launcher input: `ms:up\|down\|left\|right\|ok\|back\|lb\|rb\|start\|x\|y\|tap:X:Y\|key:NAME\|pad:xbox\|…;…`. |
| `ZIGGURAT_STATS_IMPORT` | Start a stats import of this file when the SAVES page opens. |
| `ZUI_DEBUG` | Log focus-navigation decisions. |
| `ZUI_OVERFLOW_LOG` | Log any text that is clipped or overflows its widget (used by `tools/ui_sweep.py`). |

### Steam / shortcuts (sandbox only)
| Variable | Effect |
|---|---|
| `ZIGGURAT_STEAM_ROOT` | Use this folder as the Steam installation (a sandbox copy). The launcher flag `--steam-root` and `Steam::setRootOverride()` do the same. |
| `ZIGGURAT_STEAM_FAKE_RUNNING` | In sandbox mode, pretend Steam is running (`zigsteam --fake-running` uses `Steam::setFakeRunning()` instead). |
| `ZIGGURAT_SHORTCUT_DIR` | Create desktop/Start-menu shortcuts in this folder (`zigsteam shortcut --dir` uses `Shortcuts::setDirOverride()` instead). |

The test tools (`zigsteam`, `zigextras`, `zigstats`, `zigimport`, `ziginputtest`) use explicit APIs or command-line sandboxes rather than environment variables, so they behave the same in both configurations. `zigsteam` refuses to write without `--steam-root <sandbox>` / `--dir <sandbox>`.

## Self-test executables (no game files needed, CI-safe)
```
port/build/ziginputtest.exe          # controller → virtual finger mapping (50 tests)
port/build/zigsteam.exe selftest     # binary VDF round-trips, shortcut ids, sandbox add/remove/undo
port/build/zigextras.exe selftest    # Steam Input localconfig edits, update-check parsing, overrides
port/build/zigstats.exe make-test-sqlite <out.sqlite> <stats.json>   # synthetic CoreData store for import tests
python -I tools/check_strings.py     # launcher translations: ids, placeholders, length
python -I tools/check_game_text.py   # no game text in the source (IDs only, see core/GameText.h)
```

## Tests that need YOUR imported game files (local only, never in CI)
Pass the imported assets with `--assets <dir>`, or set `ZIGGURAT_TEST_ASSETS`. The default is `port/assets`, created by `tools/extract_assets.py`. All drivers make portable copies under `<build>/test-runs/`.

```
python -I tools/tests/determinism.py [--frames 3600] [variant …]     # base, fill=0xNN, perturb=N, dir=name
python -I tools/tests/enhancement_cases.py [case … | all]           # flash shake logos god level overlay
                                                                    # snapshot seed statsfix pounce ja default
python -I tools/tests/overrides_cases.py                            # empty overrides = identical; override used
python -I tools/ui_sweep.py [--langs en,de,…]                       # every launcher page × language, overflow log
python -I tools/check_game_text.py --dump port/assets                # every text ID resolves in your dump
zigimport verify <ZiGGURAT.ipa>                                     # 188-file SHA-256 manifest
python -I tools/package.py --selftest <ZiGGURAT.ipa>                # release zip: import, boot, hooks off, launcher
```
Runtime DLLs are taken from next to the executables, then `$MINGW_PREFIX/bin`, then `where SDL3.dll`.

## Paths with non-ASCII characters
All file access goes through UTF-8-safe helpers (`port/src/core/FileIO.h`: `_wfopen`/`std::filesystem::u8path` on Windows). Both executables also embed a manifest with `activeCodePage = UTF-8` (`port/res/ziggurat.manifest`). To re-test:
* run the game and the launcher from a folder such as `àè ü 日本`
* import an IPA stored at a path with accents
* add a Steam shortcut in a sandbox at such a path
* export and import stats there
* run `determinism.py` with `dir=àè`
