# Data, records and backups

## Locations
| | Windows | Linux |
|---|---|---|
| normal | `%APPDATA%\ZigguratRecomp\` | `$XDG_DATA_HOME/ZigguratRecomp/` (usually `~/.local/share/ZigguratRecomp/`) |
| portable mode | `<launcher folder>\data\` | same |

Portable mode is enabled by an empty file named `portable.txt` next to `ZigguratLauncher`, or by `--portable`.

| File / folder | Content |
|---|---|
| `config.json` | all launcher and game settings (unknown keys are preserved) |
| `assets/` | the imported, converted game files (made from your dump; never share them) |
| `overrides/` | optional replacement files with the same names as in `assets/` (texture/sound packs); empty = no effect |
| `statistics.txt` | your records (the original CoreData "Statistics" attributes) |
| `defaults.tsv` | the game's own options (control scheme, aiming guide, tutorial seen, …) |
| `backups/` | automatic record backups `stats-<timestamp>.json` (newest 5) |
| `logs/` | `game-*.log`, `launcher-*.log` (newest 10 each) |

## Records (launcher → DATA)
* **YOUR RECORDS:** the full statistics view, with the game's own labels.
* **SAVES & BACKUPS:**
  * **Export** to a JSON file (`"format": "ziggurat-stats"`).
  * **Import** from:
    * an exported JSON file
    * a touchHLE save (`HighScore.sqlite.touchHLE.plist`)
    * the original game's `HighScore.sqlite`, or an **unencrypted** iPhone/iTunes/Finder backup folder (it is located inside the backup automatically)
  * Merge mode **Replace** or **Keep best** (totals summed, bests kept, averages weighted).
  * A preview shows the current and resulting numbers before anything is written.
  * Backups are made automatically before every import, reset or restore; restore from the list.
  * **Reset records** asks twice.
* Command line: `zigstats [--portable] show|export|import|import-touchhle|import-sqlite|backups|restore|reset`.

## Removing everything
* Delete the data folder.
* If you added the game to Steam, use **REMOVE** in the STEAM tab first.
