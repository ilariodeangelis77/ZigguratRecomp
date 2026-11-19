# Changelog

All notable changes to this project are documented here. Versions follow semantic versioning.

## [0.9.0]
First public release.

### Game
* Native C++/SDL3 reconstruction of ZiGGURAT (iOS, 2012) on a C++ translation of flixel-ios. Original logic, timings and quirks are kept by default.
* 30 fps (original iPhone) or 60 fps (retina iPhones).
* Display:
  * windowed / borderless / exclusive fullscreen, monitor choice
  * integer scaling, nearest / sharp-bilinear / linear filters
  * border art, vsync, FPS counter
* Audio: master / music / effects volumes, output device, mute in background.
* Controllers: a virtual finger for precision and slingshot mode, a menu cursor, remapping, deadzones, response curve, gyro aiming, optional rumble.
  * Aim mode "stick direction": the shot goes where the stick points (both schemes); tilt to charge, let go to fire.
* The game opens only its own window (no console window).
* Mouse and touch input behave like the iPhone's touchscreen: positions are held at the screen edge while dragging into the borders or outside the window, and presses outside the game picture are ignored. Before this, a mouse drag could aim further down than the original allows.
* Enhancements and accessibility, all off by default:
  * reduce flashing, screen-shake scale
  * fixes for original bugs
  * skip logos, death snapshot
  * practice mode (god mode, start level; records not saved)
  * developer overlay
* Assets can be overridden from `<data>/overrides/`.
* All of the game's text (menus, tutorial, stats, game over, credits) is read from your own copy, in all 7 languages. The source refers to it only by ID (`port/src/core/GameText.h`), and `tools/check_game_text.py` keeps it that way.
* Our own pixel-art "Z" icon.

### Launcher
* First-run wizard: select an `.ipa`, a `.app` or a folder, with file dialogs, drag & drop or the built-in browser. Verification against a 188-file SHA-256 manifest, clear error messages, then import.
* Tabs: HOME, DISPLAY (with Apply & test), AUDIO, CONTROLS (with a live test area), LANGUAGE, GAME (enhancements, accessibility, practice), DATA (game files, records, saves & backups), STEAM, ABOUT.
* Steam:
  * one-click add / update / remove / undo, with backups
  * generated artwork (capsule, wide, hero, logo, icon)
  * optional logo position and Steam Input mode
  * desktop and Start-menu shortcuts
* Records:
  * full stats view
  * JSON export/import, automatic backups, reset
  * import from a touchHLE save or from an unencrypted iPhone/iTunes backup
* 7 languages, with accented capitals composed for the pixel font.
* Crash panel with log tail; copy support info (anonymised).
* Portable mode (`portable.txt`).
* Optional update check (off by default).
