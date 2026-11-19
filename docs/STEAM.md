# Steam integration

Launcher → **STEAM**. Nothing happens to your Steam installation until you press a button.

## What "ADD TO STEAM" does
1. Finds Steam:
   * Windows: registry `HKCU\Software\Valve\Steam\SteamPath`
   * Linux: `~/.local/share/Steam`, `~/.steam/steam`, Flatpak `~/.var/app/com.valvesoftware.Steam/data/Steam`
2. Finds your account (`config/loginusers.vdf` + `userdata/<accountid>`).
3. Refuses to continue while Steam is running. Steam would overwrite the file when it exits. **CLOSE STEAM** runs `steam -shutdown` for you, only when you click it.
4. Backs up the files it is about to change into `userdata/<accountid>/config/ziggurat-backups/<timestamp>/` (the newest 5 are kept).
5. Adds or updates **one** non-Steam shortcut in `userdata/<accountid>/config/shortcuts.vdf`:
   * name `ZiGGURAT`; target `ZigguratLauncher` with `--from-steam`
   * tags Action, Arcade
   * all other entries are kept byte for byte
6. Writes the artwork to `userdata/<accountid>/config/grid/`. It is generated on your PC from your own dump; the icon is the project's own "Z".

| File | Size | Content |
|---|---|---|
| `<id>p.png` | 600×900 | portrait capsule |
| `<id>.png` | 920×430 | wide capsule |
| `<id>_hero.png` | 1920×620 | library banner (no logo) |
| `<id>_logo.png` | 768×170 | logo |
| `<id>_icon.png` | 256×256 | icon |

`<id>` is Steam's shortcut ID: `crc32(exe + name) | 0x80000000`, the same formula Steam ROM Manager uses.
It has been confirmed against a real Steam install.

## Other buttons
* **UPDATE IN STEAM / REMOVE / UNDO:** UNDO restores the most recent backup exactly.
* **START STEAM**, **COPY LAUNCH COMMAND** (for manual setups).
* **Desktop / Start menu shortcuts** (Windows `.lnk`, Linux `.desktop`).
* **When started from Steam**: show the launcher (default) or start the game directly.

## Experimental options
* **Logo position:** writes `grid/<id>.json` (`{"nVersion":1,"logoPosition":{...}}`). The position values
  match Steam's client API (BottomLeft, UpperLeft, CenterCenter, UpperCenter, BottomCenter); the file
  wrapper is unverified. If Steam has written its own file (after you use *Adjust Logo Position* in Steam),
  the launcher shows it read-only and never deletes it.
* **Steam Input mode:** sets `UseSteamControllerConfig` for the shortcut in `localconfig.vdf`
  (default / force on / off), editing only that one value, with a backup.

## What non-Steam games get
The Steam overlay, Steam Input, screenshots and recording. No achievements or Steam Cloud.
