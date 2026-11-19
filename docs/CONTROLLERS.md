# Controllers, keyboard and mouse

ZiGGURAT is a touch game. Every input device drives **one virtual finger** during gameplay, or a **menu
cursor** (drawn with the game's reticle) in menus. The game logic itself is unchanged: it sees exactly
the touches the original saw. Whichever device you used last takes over, and there is never more than one finger.

## Control schemes (set in the game's Options or the launcher's GAME tab)
* **Precision**:
  * Hold *press* to put the finger down at the touch band near the bottom of the screen.
  * Slide it left/right to aim (the original angle formula depends only on the finger's X position).
  * Release to fire.
  * Aim mode **direct** is an alternative: the right stick's angle sets the aim.
* **Slingshot**:
  * *Press* puts the finger down inside the original 50-pixel start circle around the hero.
  * The stick pulls it back.
  * Release to fire. Pulling further gives more power.

## Aim mode "stick direction" (launcher → CONTROLS → AIM MODE)
The shot goes **where the stick points**, in both control schemes:
* **Charge and fire:** a charge starts on a fresh tilt (past ~35 %) or a fresh trigger press. It fires as soon as you release **either** one: let the stick spring back below ~20 %, or let go of the trigger. With the trigger held, every flick of the stick fires a new shot.
* **Precision:** the stick angle is converted back to the finger position the original formula expects. Straight-down directions can't be aimed in the original either, so they clamp to the nearest edge.
* **Slingshot:** the finger is placed on the opposite side of the hero, so the pull sends the shot along the stick. Power follows how far you tilted.

## Default bindings
| Action | Controller | Keyboard |
|---|---|---|
| press (finger down) | RT | Space |
| press (second button) | A | - |
| slide / pull | left stick, d-pad (slingshot: either stick) | A / D (left/right), W / S (up/down) |
| recenter aim | right-stick click | R |
| gyro while held (gyro mode "hold") | LB | - |
| menu tap | A | mouse click |

The mouse and touch screens work like a finger. "Mouse as finger" can be switched off.

## Launcher → CONTROLS
* **Sticks & aim:**
  * controller choice (auto = last used), button icons (auto / Xbox / PlayStation / Nintendo)
  * precision aim mode, sensitivity
  * inner left/right deadzones, outer deadzone, response curve (linear / quadratic)
  * menu cursor speed
* **Gyro & rumble:**
  * gyro aiming (DualSense, DualShock 4, Switch Pro, Joy-Con, Steam Deck) always or while holding LB, with sensitivity and inversion
  * **rumble** is an enhancement and off by default (the original never vibrates): hit / charge / death / explosion
* **Buttons:** remap every action. Press X/Enter on an entry, then the new button or key (5 s timeout). Duplicate gameplay bindings get a warning.
* **Test:** a live preview of the precision aim line or the slingshot pull, using the same code as the game.

Settings live in the `input` section of `config.json`. Steam Input also works on top: it presents a standard gamepad.
