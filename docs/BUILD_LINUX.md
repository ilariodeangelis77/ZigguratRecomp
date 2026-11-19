# Building the ZiGGURAT port on Linux / Steam Deck

Status: **CI build configured, not yet play-tested on Linux.** Every Windows-only call in `port/src/core`
(registry, process list, `.lnk` shortcuts, WinHTTP) is behind `#ifdef _WIN32`, with a Linux branch for each:

| Feature | Linux implementation |
|---|---|
| Steam detection | `~/.local/share/Steam`, `~/.steam/steam`, and the Flatpak root `~/.var/app/com.valvesoftware.Steam/data/Steam` |
| Steam running check | `/proc/*/comm` |
| Desktop / menu shortcuts | `.desktop` files |
| Update check | Disabled, with a message: no HTTP client on Linux yet |
| Executable names | `ziggurat` and `ZigguratLauncher`, without `.exe` |

Linux builds are configured in CI (`.github/workflows/build.yml`, Ubuntu 24.04, SDL3 fetched with
`ZIGGURAT_FETCH_SDL3=ON`). They have not yet been play-tested on real Linux hardware or a Steam Deck.

## 1. Dependencies (Debian / Ubuntu / SteamOS dev environment)
```sh
sudo apt install build-essential cmake ninja-build pkg-config git \
     libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev libxtst-dev \
     libwayland-dev libxkbcommon-dev libegl-dev libgl-dev \
     libasound2-dev libpulse-dev libpipewire-0.3-dev libudev-dev libdbus-1-dev libdrm-dev libgbm-dev
```
SDL3 is packaged as `libsdl3-dev` on Debian 13 (trixie) and Ubuntu 25.04 or newer. Elsewhere, let CMake fetch and
build the pinned SDL3 release (`release-3.2.20`) with `-DZIGGURAT_FETCH_SDL3=ON`.

## 2. Build
```sh
cmake -S port -B port/build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release   # add -DZIGGURAT_FETCH_SDL3=ON if needed
cmake --build port/build-linux
```
Targets: `ziggurat` (game), `ZigguratLauncher`, `zigimport`, `zigsteam`, `zigstats`, `ziginputtest`, `zigextras`.
The post-build step copies `fonts/` and `strings/` next to the launcher.

## 3. Run
```sh
cd port/build-linux && ./ZigguratLauncher
```
The launcher asks for your `ZiGGURAT.ipa` (or the `Zig2.app` folder) and imports it to `~/.local/share/ZigguratRecomp/`.
Put an empty `portable.txt` next to the executable to keep everything in `./data`.

## 4. Steam Deck
* Build as above, inside a Debian/Arch container (e.g. distrobox), or on another Linux PC.
* Copy the build folder to the Deck.
* In Desktop Mode, run the launcher once and use **STEAM → ADD TO STEAM**. The launcher detects Flatpak Steam paths too.
* In Game Mode, the launcher starts fullscreen and works with the controller alone (`SteamDeck=1` is detected).
* Native file dialogs may not open in Game Mode. Use **BROWSE HERE**, the built-in file browser.

## 5. Known gaps to check on first Linux build
* `-Wall` warnings from code paths never compiled on Linux yet (e.g. unused Windows-only helpers).
* `SDL_ShowOpenFileDialog` needs a desktop portal (xdg-desktop-portal) on Wayland.
* Rumble and gyro go through SDL's hidapi drivers, which may need udev rules for some controllers (SDL documents them).
