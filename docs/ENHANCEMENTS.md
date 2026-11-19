# Enhancements, accessibility and practice

Everything here is **off by default**: the default game behaves like the original, including its bugs.
In the launcher, these settings are under **GAME → ACCESSIBILITY / ORIGINAL BUGS / PRACTICE / DEVELOPER**.
They are stored in the `enhancements` section of `config.json`.

## Presets
| Preset | Effect |
|---|---|
| **Original** | everything off |
| **Recommended** | accessibility on + the statistics bug fixes. The red-pounce fix stays off because it changes difficulty. |
| **Custom** | set automatically as soon as you change any single option |

## Accessibility
| Option | Original behaviour | With the option |
|---|---|---|
| `reduce_flashing` | full-brightness white-outs (moon explosion), lightning flashes and a full red death screen | dimmed to about 30 % |
| `screen_shake` (0–1) | full screen shake | shake intensity scaled |

A one-time photosensitivity notice is shown in the launcher. It links to **Reduce flashing**.

## Original bugs (individual fixes)
| Option | Original behaviour | Fixed behaviour |
|---|---|---|
| `fix_purple_double_count` | purple kills are added twice to the total statistics | counted once |
| `fix_yellow_as_red` | yellow aliens killed by toppling aliens are counted as **red** kills | counted as yellow kills |
| `fix_red_pounce` | the red alien's pounce timer is never reset, so it pounces again on every landing | timer reset after each pounce. **Changes difficulty.** |
| `show_ja_overlay` | the Options screen builds Japanese text images but never shows them | shown when the language is Japanese |

## Convenience
| Option | Effect |
|---|---|
| `skip_logos` | start at the title screen, skipping the Action Button / Freshuu / Flixel logos |
| `show_death_snapshot` | the original captures the last frame before death but never draws it; this shows it, dimmed, behind the game-over text |

## Practice (records are NOT saved while active)
| Option | Effect |
|---|---|
| `god_mode` | the player cannot die (the original game code contains this mode) |
| `start_enemy_level` | start at a later difficulty / background level |

## Developer (hidden until enabled)
| Option | Effect |
|---|---|
| `show_overlay` | FPS, alien/bullet counters, levels |
| `touch_zones` | draws the precision touch band or the slingshot start circle and pull radius |
| `fixed_seed` | seeds the game's random generator, for reproducible runs |
| `log_level` | `info` / `debug` / `verbose` |
