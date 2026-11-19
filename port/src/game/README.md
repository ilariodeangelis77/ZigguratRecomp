# port/src/game: game logic reconstruction

This directory contains a C++ reconstruction of the gameplay and menu logic of
**ZiGGURAT** (iOS, 2012, Action Button Entertainment), written so that the game can
run on current systems using the player's own copy of the original game data.

* It is provided for **interoperability and preservation**. It contains no game
  assets (art, audio, fonts, text tables). Those are read at runtime from the user's own dump.
* All rights to the original game, including its code, design, art, audio, text,
  name and trademarks, belong to their respective owners. This project is
  unofficial and not affiliated with or endorsed by them.
* The project licence (see `/LICENSE`) applies **only to the original contributions
  of this project**. It does not grant any rights to the original game.
* Comments above each function name the original Objective-C selector and address,
  so behaviour can be compared against the original. Known original bugs are
  reproduced by default. Optional fixes are listed in `docs/ENHANCEMENTS.md`.
