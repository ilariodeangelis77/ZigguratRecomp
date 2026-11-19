# Security policy

## What the software touches on your system
* **Launcher → Steam (only when you click the buttons in the STEAM tab):**
  * writes `userdata/<account>/config/shortcuts.vdf` and artwork in `userdata/<account>/config/grid/`
  * optionally edits the per-app Steam Input setting in `localconfig.vdf`
  * runs `steam -shutdown` when you click **CLOSE STEAM**

  Every write is preceded by a backup (`config/ziggurat-backups/`), and **UNDO** restores it. Nothing is written while Steam is running.
* **Launcher → shortcuts:** creates or removes Desktop / Start-menu shortcuts on request.
* **Game and launcher:** read and write only their own data folder (see `docs/DATA.md`) and the dump you select.
* **Network:** none, except the optional update check (off by default; it only downloads a small JSON file and never installs anything).

## Reporting a vulnerability
Please **do not** open a public issue. Use GitHub's private reporting instead: *Security → Report a
vulnerability* on this repository (GitHub Security Advisories). Include the version, OS, and steps to
reproduce. Do not attach game files.
