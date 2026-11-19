# Building on Windows (MSYS2 / MinGW-w64)

1. Install [MSYS2](https://www.msys2.org/) and open the **MSYS2 MINGW64** shell.
2. Install the toolchain and SDL3:
   ```sh
   pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja \
                      mingw-w64-x86_64-sdl3 mingw-w64-x86_64-python
   ```
3. Configure and build:
   ```sh
   cmake -S port -B port/build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build port/build
   ```

| Option | Default | Meaning |
|---|---|---|
| `ZIGGURAT_FETCH_SDL3` | OFF | fetch and build the pinned SDL3 release (`release-3.2.20`) instead of the system package |
| `ZIGGURAT_TEST_HOOKS` | ON | compile in the `ZIGGURAT_*` environment-variable test hooks and developer-only launcher screens. Release builds use `-DZIGGURAT_TEST_HOOKS=OFF` |

**Outputs in `port/build/`:**
* `ZigguratLauncher.exe` and `ziggurat.exe`, plus `fonts/` and `strings/`
* the test and maintenance CLIs: `zigimport`, `zigstats`, `zigsteam`, `zigextras`, `ziginputtest`

**Running from the build folder:** put `SDL3.dll` and `libwinpthread-1.dll` from `mingw64/bin` next to the executables, or run them from the MINGW64 shell.
Then start `ZigguratLauncher.exe` and select your own dump.

**Packaging:** `python -I tools/package.py` builds `dist/ZigguratRecomp-<version>-windows-x64.zip`, audits it
for game data and runs a self-test. It finds MinGW through `MINGW_PREFIX`, which the MSYS2 shell sets.

**Tests:** see `CONTRIBUTING.md`.
