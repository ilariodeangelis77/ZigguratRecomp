#include "launcher/Strings.h"

#include <SDL3/SDL.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

#include "core/Paths.h"
#include "json.hpp"

namespace lz {
namespace {

// id -> English. Keep ids stable: translation files use them.
const std::map<std::string, std::string>& english() {
    static const std::map<std::string, std::string> m = {
        // generic
        {"app.title", "ZIGGURAT LAUNCHER"},
        {"app.unofficial", "UNOFFICIAL FAN PORT"},
        {"btn.ok", "OK"}, {"btn.cancel", "CANCEL"}, {"btn.back", "BACK"}, {"btn.next", "NEXT"},
        {"btn.yes", "YES"}, {"btn.no", "NO"}, {"btn.close", "CLOSE"}, {"btn.retry", "TRY AGAIN"},
        {"btn.play", "PLAY NOW"}, {"btn.defaults", "DEFAULTS"}, {"btn.keep", "KEEP"}, {"btn.revert", "REVERT"},
        {"prompt.select", "SELECT"}, {"prompt.back", "BACK"}, {"prompt.tabs", "TABS"}, {"prompt.defaults", "DEFAULTS"},
        {"prompt.play", "PLAY"},
        // tabs
        {"tab.home", "HOME"}, {"tab.display", "DISPLAY"}, {"tab.audio", "AUDIO"}, {"tab.controls", "CONTROLS"},
        {"tab.language", "LANGUAGE"}, {"tab.game", "GAME"}, {"tab.data", "DATA"}, {"tab.about", "ABOUT"},
        // wizard
        {"wiz.welcome.title", "WELCOME"},
        {"wiz.welcome.text",
         "This is an unofficial fan port of ZiGGURAT (2012, iOS) by Action Button Entertainment. "
         "It needs your own copy of the game: the launcher reads the art, music and text from it. "
         "No game files are included."},
        {"wiz.language", "LANGUAGE"},
        {"wiz.locate.title", "FIND YOUR GAME"},
        {"wiz.locate.text",
         "Select your ZiGGURAT .ipa file or the Zig2.app folder. You can also drag it onto this window. "
         "Encrypted App Store copies work too."},
        {"wiz.choose_ipa", "CHOOSE .IPA FILE"}, {"wiz.choose_folder", "CHOOSE FOLDER"},
        {"wiz.browse", "BROWSE HERE"},
        {"wiz.choose_ipa.help", "Open a file picker for the .ipa file."},
        {"wiz.choose_folder.help", "Pick a folder: the launcher searches it for the game."},
        {"wiz.browse.help", "Built-in file browser (works with a controller)."},
        {"wiz.scanning", "LOOKING FOR THE GAME..."},
        {"wiz.verify.title", "CHECKING FILES"},
        {"wiz.verify.ok", "ZiGGURAT {0} recognised. All {1} files OK."},
        {"wiz.import_anyway", "IMPORT ANYWAY"},
        {"wiz.import.title", "IMPORTING"},
        {"wiz.import.text", "Converting the game files for the port..."},
        {"wiz.import.done", "Done! {0} files imported."},
        {"wiz.setup.title", "QUICK SETUP"},
        {"wiz.setup.display", "DISPLAY"},
        {"wiz.setup.display.help", "How the game window starts. You can change it later in DISPLAY."},
        {"wiz.done.title", "READY!"},
        {"wiz.done.text", "Everything is set. Have fun, and shoot the eyes."},
        {"wiz.choose_other", "CHOOSE ANOTHER"},
        {"preset.win2", "WINDOW X2"}, {"preset.win3", "WINDOW X3"}, {"preset.borderless", "FULL SCREEN"},
        // file browser
        {"fb.title", "SELECT .IPA OR .APP"}, {"fb.up", "[ UP ONE LEVEL ]"}, {"fb.use_folder", "USE THIS FOLDER"},
        {"fb.places", "PLACES"}, {"fb.help", "Enter a folder, or pick an .ipa / .app. Y uses the current folder."},
        {"fb.home", "HOME"}, {"fb.desktop", "DESKTOP"}, {"fb.downloads", "DOWNLOADS"}, {"fb.documents", "DOCUMENTS"},
        // home
        {"home.files_ok", "GAME FILES: OK (V{0})"}, {"home.files_missing", "GAME FILES: MISSING"},
        {"home.controller", "CONTROLLER: {0}"}, {"home.no_controller", "CONTROLLER: NONE (MOUSE = FINGER)"},
        {"home.data", "DATA: {0}"}, {"home.portable", "PORTABLE MODE"},
        {"home.stats", "YOUR RECORDS"}, {"home.games", "GAMES PLAYED"}, {"home.best_kills", "BEST KILLS"},
        {"home.best_seconds", "BEST TIME"}, {"home.total_kills", "TOTAL KILLS"},
        {"home.no_stats", "No games played yet."},
        {"home.play.help", "Start ZiGGURAT with the current settings."},
        {"home.running", "THE GAME IS RUNNING..."},
        // display
        {"disp.mode", "DISPLAY MODE"}, {"disp.mode.help", "Window, borderless full screen (recommended) or exclusive full screen."},
        {"disp.mode.windowed", "WINDOWED"}, {"disp.mode.borderless", "BORDERLESS"}, {"disp.mode.exclusive", "EXCLUSIVE"},
        {"disp.monitor", "MONITOR"}, {"disp.monitor.help", "Which display the game opens on."},
        {"disp.size", "WINDOW SIZE"}, {"disp.size.help", "Multiples of the original 480x320 iPhone screen."},
        {"disp.fsres", "FULL SCREEN RES"}, {"disp.fsres.help", "Resolution for exclusive full screen. Desktop is safest."},
        {"disp.fsres.desktop", "DESKTOP"},
        {"disp.integer", "INTEGER SCALING"}, {"disp.integer.help", "Keep every pixel the same size (sharpest)."},
        {"disp.filter", "FILTER"}, {"disp.filter.help", "Used when integer scaling is off. Sharp-smooth avoids uneven pixels."},
        {"disp.filter.nearest", "SHARP PIXELS"}, {"disp.filter.sharp", "SHARP-SMOOTH"}, {"disp.filter.linear", "SMOOTH"},
        {"disp.aspect", "ASPECT"}, {"disp.aspect.help", "Original 3:2 picture, or stretch to fill the screen."},
        {"disp.aspect.original", "ORIGINAL 3:2"}, {"disp.aspect.stretch", "STRETCH"},
        {"disp.borders", "BORDERS"}, {"disp.borders.help", "What fills the space around the game picture."},
        {"disp.borders.black", "BLACK"}, {"disp.borders.night", "NIGHT SKY"}, {"disp.borders.sunset", "SUNSET"},
        {"disp.borders.blur", "DARK SKY"},
        {"disp.vsync", "V-SYNC"}, {"disp.vsync.help", "Synchronise with the display refresh to avoid tearing."},
        {"disp.fps", "FRAME RATE"}, {"disp.fps.help", "30 = original iPhone, 60 = retina iPhones. Both are authentic."},
        {"disp.fps.30", "30 (ORIGINAL)"}, {"disp.fps.60", "60 (RETINA)"},
        {"disp.showfps", "SHOW FPS"}, {"disp.showfps.help", "Frame counter in the corner of the game."},
        {"disp.test", "APPLY & TEST"}, {"disp.test.help", "Start the game for 5 seconds with these settings."},
        {"disp.keep.title", "KEEP THESE SETTINGS?"},
        {"disp.keep.text", "Did the game look right? Keep the new display settings or go back to the previous ones."},
        // audio
        {"aud.master", "MASTER VOLUME"}, {"aud.master.help", "Overall game volume."},
        {"aud.music", "MUSIC VOLUME"}, {"aud.music.help", "Music volume (multiplies the game's own setting)."},
        {"aud.sfx", "EFFECTS VOLUME"}, {"aud.sfx.help", "Sound effects volume."},
        {"aud.device", "OUTPUT DEVICE"}, {"aud.device.help", "Where the game plays sound."},
        {"aud.device.default", "DEFAULT"},
        {"aud.mute", "MUTE IN BACKGROUND"}, {"aud.mute.help", "Silence the game when its window is not focused."},
        {"aud.launcher_music", "LAUNCHER MUSIC"}, {"aud.launcher_music.help", "Play the title music in this launcher."},
        {"aud.test_sound", "TEST SOUND"}, {"aud.test_sound.help", "Play a game sound effect."},
        {"aud.test_music", "TEST MUSIC"}, {"aud.test_music.help", "Play 5 seconds of the title music."},
        // controls
        {"ctl.title", "CONTROLS"},
        {"ctl.text", "Mouse, touch, keyboard and controllers all drive one virtual finger, exactly like on the iPhone."},
        {"ctl.page", "PAGE"}, {"ctl.page.help", "Controller settings are split into pages."},
        {"ctl.page.sticks", "STICKS & AIM"}, {"ctl.page.gyro", "GYRO & RUMBLE"},
        {"ctl.page.buttons", "BUTTONS"}, {"ctl.page.test", "TEST"},
        {"ctl.device", "CONTROLLER"}, {"ctl.device.help", "Which controller plays. AUTO = the one you used last."},
        {"ctl.device.auto", "AUTO"},
        {"ctl.glyphs", "BUTTON ICONS"}, {"ctl.glyphs.help", "Button prompts shown in this launcher."},
        {"ctl.glyphs.auto", "AUTO"},
        {"ctl.aim", "AIM MODE"},
        {"ctl.aim.help", "SLIDE: trigger + slide (original). DIRECT: right stick aims. STICK: shoot where the stick points, tilt to charge, let go to fire."},
        {"ctl.aim.slide", "SLIDE (ORIGINAL)"}, {"ctl.aim.direct", "DIRECT"}, {"ctl.aim.stick", "STICK DIRECTION"},
        {"ctl.sens", "SENSITIVITY"}, {"ctl.sens.help", "Finger speed when sliding, and pull distance in slingshot mode."},
        {"ctl.dzl", "LEFT DEADZONE"}, {"ctl.dzl.help", "Ignore small left stick movements."},
        {"ctl.dzr", "RIGHT DEADZONE"}, {"ctl.dzr.help", "Ignore small right stick movements."},
        {"ctl.outer", "OUTER DEADZONE"}, {"ctl.outer.help", "How far you must push for full speed."},
        {"ctl.curve", "RESPONSE CURVE"}, {"ctl.curve.help", "Quadratic gives finer control near the centre."},
        {"ctl.curve.linear", "LINEAR"}, {"ctl.curve.quadratic", "QUADRATIC"},
        {"ctl.cursor", "MENU CURSOR SPEED"}, {"ctl.cursor.help", "Speed of the cursor used in the game's menus (A taps)."},
        {"ctl.mouse", "MOUSE AS FINGER"}, {"ctl.mouse.help", "Press = touch, drag = slide, release = lift."},
        {"ctl.gyro", "GYRO AIM"}, {"ctl.gyro.help", "Turn the controller to move the finger (DualSense, Switch Pro, Steam Deck...)."},
        {"ctl.gyro.mode", "GYRO MODE"}, {"ctl.gyro.mode.help", "ALWAYS, or only while the GYRO HOLD button is held."},
        {"ctl.gyro.always", "ALWAYS"}, {"ctl.gyro.hold", "WHILE HOLDING"},
        {"ctl.gyro.sens", "GYRO SENSITIVITY"}, {"ctl.gyro.sens.help", "How far the finger moves per turn."},
        {"ctl.gyro.ix", "INVERT GYRO X"}, {"ctl.gyro.ix.help", "Reverse left/right turning."},
        {"ctl.gyro.iy", "INVERT GYRO Y"}, {"ctl.gyro.iy.help", "Reverse up/down turning (slingshot)."},
        {"ctl.rumble", "RUMBLE (ENHANCEMENT)"},
        {"ctl.rumble.help", "The original never vibrated. Off = authentic. On: hits, charging and death rumble."},
        {"ctl.rumble.test", "TEST RUMBLE"}, {"ctl.rumble.test.help", "Vibrate the controller at this strength."},
        {"ctl.rumble.off", "Rumble is off."},
        {"ctl.bind.help", "LEFT/RIGHT: choose a button. X (or Enter): press the button to assign."},
        {"ctl.bind.press", "PAD: PRESS (FINGER)"}, {"ctl.bind.press_alt", "PAD: PRESS (2ND)"},
        {"ctl.bind.menu_tap", "PAD: MENU TAP"}, {"ctl.bind.recenter", "PAD: RECENTER AIM"},
        {"ctl.bind.gyro_hold", "PAD: GYRO HOLD"},
        {"ctl.key.press", "KEY: PRESS (FINGER)"}, {"ctl.key.left", "KEY: SLIDE LEFT"},
        {"ctl.key.right", "KEY: SLIDE RIGHT"}, {"ctl.key.up", "KEY: PULL UP"}, {"ctl.key.down", "KEY: PULL DOWN"},
        {"ctl.key.recenter", "KEY: RECENTER AIM"},
        {"ctl.none", "NONE"},
        {"ctl.capture", "PRESS A BUTTON FOR {0}... ({1})"},
        {"ctl.capture.key", "PRESS A KEY FOR {0}... ESC CANCELS ({1})"},
        {"ctl.conflict", "WARNING: {0} IS USED TWICE"},
        {"ctl.test.text", "Try it: hold your PRESS button and move the stick. Uses your current control scheme."},
        {"ctl.test.area", "TEST AREA"}, {"ctl.test.area.help", "Live preview of the virtual finger."},
        {"ctl.detected", "DETECTED: {0}"},
        // language
        {"lang.title", "LANGUAGE"},
        {"lang.help", "Language of the game and this launcher. The game uses it from its next start."},
        {"lang.system", "SYSTEM DEFAULT"},
        // game
        {"game.scheme", "CONTROL SCHEME"}, {"game.scheme.help", "Precision: slide to aim. Slingshot: pull back and release."},
        {"game.scheme.precision", "PRECISION"}, {"game.scheme.slingshot", "SLINGSHOT"},
        {"game.aim", "AIMING GUIDE"}, {"game.aim.help", "Show the dotted arc of your shot."},
        {"game.tutorial", "SHOW TUTORIAL"}, {"game.tutorial.help", "Show the how-to-play slides at the next start."},
        {"launch.onplay", "WHEN THE GAME STARTS"}, {"launch.onplay.help", "What this launcher does while you play."},
        {"launch.onplay.close", "CLOSE LAUNCHER"}, {"launch.onplay.minimize", "MINIMISE"}, {"launch.onplay.stay", "STAY OPEN"},
        {"launch.steam", "FROM STEAM"}, {"launch.steam.help", "When started from Steam: show this launcher, or start the game."},
        {"launch.steam.show", "SHOW LAUNCHER"}, {"launch.steam.start", "START GAME"},
        // data
        {"data.dump", "GAME DUMP"}, {"data.dump.none", "No game imported."},
        {"data.dump.ok", "ZiGGURAT {0} - verified"}, {"data.dump.unverified", "ZiGGURAT {0} - unverified version"},
        {"data.reimport", "RE-IMPORT GAME"}, {"data.reimport.help", "Select the game again (for example after moving it)."},
        {"data.remove", "REMOVE GAME FILES"}, {"data.remove.help", "Delete the imported copy (about 14 MB). Your saves stay."},
        {"data.remove.confirm", "Delete the imported game files? Your records and settings are kept."},
        {"data.open_data", "OPEN DATA FOLDER"}, {"data.open_data.help", "Settings, records and logs live here."},
        {"data.open_logs", "OPEN LOGS"}, {"data.open_logs.help", "Log files of the game and the launcher."},
        {"data.open_assets", "OPEN GAME FILES"}, {"data.open_assets.help", "The imported, converted game files."},
        {"data.support", "COPY SUPPORT INFO"}, {"data.support.help", "Copy system and settings info for a bug report (no personal paths)."},
        {"data.copied", "Support info copied to the clipboard."},
        {"data.portable.on", "Portable mode is ON: data is stored next to the launcher (data folder)."},
        {"data.portable.off", "Portable mode is OFF. Put an empty file named portable.txt next to the launcher to keep everything in one folder."},
        // data pages, records, saves (L7)
        {"data.page", "PAGE"}, {"data.page.help", "Game files, your records, or saves and backups."},
        {"data.page.files", "GAME FILES"}, {"data.page.records", "YOUR RECORDS"}, {"data.page.saves", "SAVES & BACKUPS"},
        {"rec.section", "SHOW"}, {"rec.section.help", "Your records, as on the game's stats screen."},
        {"rec.sec.totals", "TOTALS & KILLS"}, {"rec.sec.best", "AVERAGES & BEST ROUND"}, {"rec.sec.deaths", "DEATHS"},
        {"rec.empty", "No games played yet."},
        {"sv.export", "EXPORT RECORDS..."}, {"sv.export.help", "Save your records and game options to a .json file."},
        {"sv.export.fail", "Could not write the export file."}, {"sv.exported", "Exported to {0}."},
        {"sv.import", "IMPORT FILE..."}, {"sv.import.help", "Load records from an export (.json), a touchHLE save or HighScore.sqlite."},
        {"sv.touchhle", "FROM TOUCHHLE..."}, {"sv.touchhle.help", "Pick the touchHLE folder: its ZiGGURAT save is found automatically."},
        {"sv.ios", "FROM IPHONE BACKUP..."}, {"sv.ios.help", "Pick an unencrypted iTunes/Finder backup folder of your old iPhone."},
        {"sv.browse", "BROWSE HERE..."}, {"sv.browse.help", "Built-in file browser (works with a controller)."},
        {"sv.browse.title", "SELECT A RECORDS FILE OR FOLDER"},
        {"sv.reset", "RESET RECORDS"}, {"sv.reset.help", "Set every record back to zero (a backup is kept)."},
        {"sv.mode", "WHEN IMPORTING"}, {"sv.mode.help", "Replace your records, or add them up and keep the best results."},
        {"sv.mode.replace", "REPLACE"}, {"sv.mode.best", "ADD UP, KEEP BEST"},
        {"sv.settings", "ALSO IMPORT OPTIONS"}, {"sv.settings.help", "Exports also contain control scheme and aiming guide."},
        {"sv.backups", "BACKUPS (MADE BEFORE EVERY CHANGE)"}, {"sv.backups.none", "No backups yet."},
        {"sv.backups.help", "Pick a backup to restore it."}, {"sv.backup.item", "{0} GAMES, {1} KILLS"},
        {"sv.restore", "RESTORE"}, {"sv.restore.help", "Go back to the selected backup."},
        {"sv.restore.title", "RESTORE BACKUP?"}, {"sv.restore.text", "Replace your records with {0}? Your current records are backed up first."},
        {"sv.restored", "Backup restored."},
        {"sv.preview.title", "IMPORT RECORDS?"}, {"sv.preview.file", "FILE:"}, {"sv.preview.now", "NOW:"},
        {"sv.preview.after", "AFTER:"}, {"sv.preview.line", "{0} games, {1} kills, best {2} kills / {3} sec"},
        {"sv.preview.backup", "Your current records are backed up first."},
        {"sv.do_import", "IMPORT"}, {"sv.imported", "Records imported."},
        {"sv.reset.title", "RESET RECORDS?"}, {"sv.reset.text1", "All your records will be set to zero."},
        {"sv.reset.text2", "Are you sure? A backup is kept, so you can restore it later."},
        {"sv.reset.do", "RESET"}, {"sv.reset.really", "YES, RESET"}, {"sv.reset.done", "Records reset. A backup was kept."},
        {"sv.err.title", "CAN'T IMPORT"}, {"sv.done.title", "DONE"},
        {"stats.err.read", "Can't read the file."}, {"stats.err.json", "This is not a valid records file."},
        {"stats.err.format", "This is not a ZiGGURAT records file."},
        {"stats.err.version", "This records file was made by a newer launcher."},
        {"stats.err.values", "The file contains invalid numbers."},
        {"stats.err.plist", "This is not a touchHLE save file."},
        {"stats.err.nostats", "No ZiGGURAT records were found in this file."},
        {"stats.err.sqlite", "This database can't be opened."},
        {"stats.err.notable", "This database has no ZiGGURAT records."},
        {"stats.err.encrypted", "This file can't be read. If it comes from an encrypted iPhone backup, make an unencrypted backup first."},
        {"stats.err.notfound", "No ZiGGURAT save was found in this folder."},
        // about
        // steam (L6)
        {"tab.steam", "STEAM"},
        {"steam.found", "STEAM: FOUND"},
        {"steam.sandbox", " (TEST FOLDER)"},
        {"steam.notfound", "STEAM: NOT FOUND"},
        {"steam.notfound.help", "Steam doesn't seem to be installed for this user. You can still create desktop and Start menu shortcuts below."},
        {"steam.running", "STEAM IS RUNNING"},
        {"steam.closed", "STEAM IS CLOSED"},
        {"steam.account", "ACCOUNT"},
        {"steam.account.help", "Which Steam user's library gets the shortcut."},
        {"steam.noacc", "No Steam account found: log in to Steam once, then come back."},
        {"steam.in_library", "ZIGGURAT IS IN THIS STEAM LIBRARY"},
        {"steam.not_added", "ZIGGURAT IS NOT IN STEAM YET"},
        {"steam.add", "ADD TO STEAM"},
        {"steam.update", "UPDATE IN STEAM"},
        {"steam.add.help", "Adds ZiGGURAT to Steam with artwork made from your game files."},
        {"steam.account.n", "ACCOUNT {0}"},
        {"steam.remove", "REMOVE"},
        {"steam.remove.help", "Remove the ZiGGURAT shortcut and its artwork from Steam."},
        {"steam.undo", "UNDO"},
        {"steam.undo.help", "Put Steam's shortcut list back as it was before the last change."},
        {"steam.close", "CLOSE STEAM"},
        {"steam.close.help", "Ask Steam to quit. Needed before changing its shortcut list."},
        {"steam.start", "START STEAM"},
        {"steam.start.help", "Start Steam to see ZiGGURAT in your library."},
        {"steam.copycmd", "COPY COMMAND"},
        {"steam.copycmd.help", "Copy the launch command, to add the game to Steam by hand."},
        {"steam.copied", "Launch command copied to the clipboard."},
        {"steam.desktop", "DESKTOP ICON"},
        {"steam.desktop.help", "Create or remove a desktop shortcut to this launcher."},
        {"steam.startmenu", "START MENU"},
        {"steam.startmenu.help", "Create or remove a Start menu entry for this launcher."},
        {"steam.art", "STEAM ARTWORK (MADE FROM YOUR GAME FILES)"},
        {"steam.info", "Non-Steam games get the Steam overlay, Steam Input, screenshots and recording, but no achievements or cloud saves."},
        {"steam.close.title", "CLOSE STEAM?"},
        {"steam.close.text", "Steam must be closed to change its shortcut list (it rewrites the file when it quits). Close Steam now? The change is applied as soon as it has closed."},
        {"steam.running.note", "Steam is running: you'll be asked to close it first."},
        {"steam.waiting", "Waiting for Steam to close..."},
        {"steam.wait_timeout", "Steam did not close. Close it by hand and try again."},
        {"steam.ok.added", "Added! Start Steam to see ZiGGURAT in your library."},
        {"steam.ok.updated", "Steam shortcut updated."},
        {"steam.ok.removed", "Removed from Steam."},
        {"steam.ok.undone", "Last Steam change undone."},
        {"steam.err.running", "Steam is running: close it first."},
        {"steam.err.account", "Steam account folder not found."},
        {"steam.err.corrupt", "Steam's shortcut list could not be read. Nothing was changed."},
        {"steam.err.backup", "Could not make a backup. Nothing was changed."},
        {"steam.err.write", "Could not write Steam's shortcut list."},
        {"steam.err.notadded", "ZiGGURAT is not in Steam."},
        {"steam.err.noundo", "Nothing to undo."},
        {"steam.err.art", "Could not create the Steam artwork."},
        {"wiz.steam.title", "ADD TO STEAM?"},
        {"wiz.steam.text", "Steam was found on this computer. Add ZiGGURAT to your Steam library, with library artwork made from your game files? You can also do this later in the STEAM tab."},
        {"wiz.steam.skip", "NOT NOW"},
        // enhancements & accessibility (L8, LAUNCHER_PLAN.md 7.9)
        {"enh.page", "PAGE"},
        {"enh.page.help", "Game options, accessibility, fixes for original bugs and practice settings."},
        {"enh.page.game", "GAME"},
        {"enh.page.access", "ACCESSIBILITY"},
        {"enh.page.bugs", "ORIGINAL BUGS"},
        {"enh.page.practice", "PRACTICE"},
        {"enh.page.dev", "DEVELOPER"},
        {"enh.preset", "PRESET"},
        {"enh.preset.help", "Original = the 2012 game. Recommended = accessibility + stats fixes (same gameplay)."},
        {"enh.preset.original", "ORIGINAL"},
        {"enh.preset.recommended", "RECOMMENDED"},
        {"enh.preset.custom", "CUSTOM"},
        {"enh.flash", "REDUCE FLASHING"},
        {"enh.flash.help", "Dims the white-out, lightning and the red death screen to 30 %."},
        {"enh.shake", "SCREEN SHAKE"},
        {"enh.shake.help", "Strength of the screen shake (100 % = original)."},
        {"enh.logos", "SKIP INTRO LOGOS"},
        {"enh.logos.help", "Start at the title screen (the original lets you tap to skip)."},
        {"enh.snapshot", "DEATH SNAPSHOT"},
        {"enh.snapshot.help", "Show your last moment behind the game-over text. The original never draws it."},
        {"enh.access.note", "To see the tutorial again use SHOW TUTORIAL on the GAME page."},
        {"enh.bugs.note", "The port keeps the original's bugs. Turn on a fix to change only that behaviour."},
        {"enh.fixed", "FIXED"},
        {"enh.original", "ORIGINAL"},
        {"enh.fixpurple", "PURPLE KILLS COUNTED TWICE"},
        {"enh.fixpurple.help", "Original: your stats add purple kills twice after every game."},
        {"enh.fixyellow", "YELLOW KILLS COUNTED AS RED"},
        {"enh.fixyellow.help", "Original: yellow aliens crushed by a toppling alien count as red kills."},
        {"enh.fixpounce", "RED ALIENS POUNCE NONSTOP"},
        {"enh.fixpounce.help", "Original: a red alien pounces again on every landing. CHANGES DIFFICULTY."},
        {"enh.fixpounce.warn", "This fix changes difficulty: red aliens wait 1.6 s between pounces."},
        {"enh.ja", "JAPANESE OPTIONS TEXT"},
        {"enh.ja.help", "Original: the Japanese options overlay is built but never shown. Only matters with Japanese."},
        {"enh.practice.note", "PRACTICE: games played with these settings are NOT recorded in your stats."},
        {"enh.god", "GOD MODE"},
        {"enh.god.help", "Nothing can hurt you (a hidden switch in the original code)."},
        {"enh.level", "START AT ENEMY LEVEL"},
        {"enh.level.help", "Skip the early waves. Levels 5+ only follow the moon explosion."},
        {"enh.level.0", "START (ORIGINAL)"},
        {"enh.devopts", "SHOW DEVELOPER OPTIONS"},
        {"enh.devopts.help", "Adds a DEVELOPER page with debugging tools."},
        {"enh.overlay", "DEBUG OVERLAY"},
        {"enh.overlay.help", "Frame rate, aliens and bullets on screen, enemy and sky level."},
        {"enh.zones", "SHOW TOUCH ZONES"},
        {"enh.zones.help", "Draw where touches count in the current control scheme."},
        {"enh.seed", "FIXED RANDOM SEED"},
        {"enh.seed.help", "Same seed = same alien spawns every game (for testing)."},
        {"enh.seed.random", "RANDOM (ORIGINAL)"},
        {"enh.log", "LOG LEVEL"},
        {"enh.log.help", "How much the game writes to its log file."},
        {"enh.notice.title", "PHOTOSENSITIVITY"},
        {"enh.notice.text", "ZiGGURAT contains flashing lights and screen flashes (lightning, a white-out, a red death screen). You can dim them with REDUCE FLASHING in GAME > ACCESSIBILITY."},
        {"enh.notice.reduce", "REDUCE FLASHING"},
        // extras (P2): overrides, reference mode, Steam experimental, updates
        {"data.page.extras", "MODS & EXTRAS"},
        {"extras.ov.title", "LOOSE-FILE OVERRIDES (TEXTURE / SOUND PACKS)"},
        {"extras.ov.use", "USE OVERRIDES"},
        {"extras.ov.use.help", "Files in the overrides folder replace game files with the same name."},
        {"extras.ov.count", "ACTIVE OVERRIDE FILES: {0}"},
        {"extras.ov.open", "OPEN OVERRIDES FOLDER"},
        {"extras.ov.open.help", "Put replacement PNG / WAV / MP3 / TTF / strings/*.json files here."},
        {"extras.ov.info", "Use the original file names (for example sky_day.png or strings/it.json). An empty folder changes nothing. The game logs every override it uses."},
        {"extras.ref.title", "REFERENCE MODE (DEVELOPER)"},
        {"extras.ref.found", "TOUCHHLE FOUND: {0}"},
        {"extras.ref.notfound", "TOUCHHLE NOT FOUND (SET extras.touchhle_path IN CONFIG.JSON)"},
        {"extras.ref.run", "RUN ORIGINAL (TOUCHHLE)"},
        {"extras.ref.run.help", "Start the original iOS game in a local touchHLE build, for side-by-side checks."},
        {"extras.ref.started", "touchHLE started with the original game."},
        {"extras.ref.err", "Could not start touchHLE: {0}"},
        {"extras.st.page", "PAGE"},
        {"extras.st.page.lib", "LIBRARY"},
        {"extras.st.page.exp", "EXPERIMENTAL"},
        {"extras.st.page.help", "Steam library entry, or experimental extras."},
        {"extras.st.warn", "EXPERIMENTAL: FILE FORMATS NOT DOCUMENTED BY VALVE"},
        {"extras.logo", "LOGO POSITION"},
        {"extras.logo.help", "Where Steam draws the logo on the library banner. Steam default writes nothing."},
        {"extras.logo.default", "STEAM DEFAULT"},
        {"extras.logo.bl", "BOTTOM LEFT"},
        {"extras.logo.ul", "UPPER LEFT"},
        {"extras.logo.cc", "CENTER"},
        {"extras.logo.uc", "UPPER CENTER"},
        {"extras.logo.bc", "BOTTOM CENTER"},
        {"extras.logo.size", "LOGO SIZE"},
        {"extras.logo.size.help", "Logo size in percent of the banner."},
        {"extras.logo.none", "No logo position file in Steam yet."},
        {"extras.logo.detected", "Found in Steam: {0}"},
        {"extras.logo.apply", "APPLY TO STEAM"},
        {"extras.logo.apply.help", "Update the Steam shortcut (and its artwork) with this logo position."},
        {"extras.si", "STEAM INPUT"},
        {"extras.si.help", "Steam Input for this shortcut. The game reads controllers itself, so default is fine."},
        {"extras.si.default", "USE DEFAULT"},
        {"extras.si.enabled", "FORCE ON"},
        {"extras.si.disabled", "OFF"},
        {"extras.si.apply", "APPLY STEAM INPUT"},
        {"extras.si.apply.help", "Write the setting into Steam's localconfig.vdf (Steam must be closed, a backup is made)."},
        {"extras.si.ok", "Steam Input setting saved."},
        {"extras.si.err", "Could not change Steam's localconfig.vdf."},
        {"extras.st.info", "Tip: in Steam, right-click the banner and use Adjust Logo Position; the file Steam writes is shown above so the format can be checked."},
        {"extras.about.page", "PAGE"},
        {"extras.about.page.about", "ABOUT"},
        {"extras.about.page.upd", "UPDATES"},
        {"extras.about.page.help", "Credits and licences, or version and updates."},
        {"extras.upd.version", "VERSION {0}"},
        {"extras.upd.toggle", "CHECK FOR UPDATES"},
        {"extras.upd.toggle.help", "Off by default. Only asks the release feed; nothing is ever downloaded or installed."},
        {"extras.upd.nofeed", "No public release channel configured."},
        {"extras.upd.feed", "FEED: {0}"},
        {"extras.upd.check", "CHECK NOW"},
        {"extras.upd.check.help", "Ask the configured release feed for the newest version."},
        {"extras.upd.checking", "CHECKING..."},
        {"extras.upd.open", "OPEN DOWNLOAD PAGE"},
        {"extras.upd.open.help", "Opens the release page in your browser."},
        {"extras.upd.latest", "You have the latest version ({0})."},
        {"extras.upd.newer", "New version available: {0}"},
        {"extras.upd.err.net", "Could not reach the release feed."},
        {"extras.upd.err.feed", "The release feed is not valid."},
        {"extras.upd.info", "The feed URL is set with extras.update_feed in config.json. The launcher only shows the release notes and opens the download page."},
        {"about.title", "ABOUT"},
        {"about.game", "ZiGGURAT (C) 2012 ACTION BUTTON ENTERTAINMENT"},
        {"about.staff", "GAME DESIGN / DIRECTOR: TIM ROGERS\nPROGRAM: MICHAEL KERWIN\nGRAPHICS: BRENT PORTER\nSOUND: ANDREW TOUPS"},
        {"about.thanks", "SPECIAL THANKS: BENNETT FODDY, TRACEY LIEN, ZAK MCCUNE"},
        {"about.port", "PORT: reconstructed from the iOS release for preservation. Engine: flixel-ios (MIT, Semi Secret Software)."},
        {"about.licences", "LICENCES: flixel-ios (MIT), SDL3 (zlib), stb, dr_libs, SQLite (public domain), nlohmann/json (MIT), Silkscreen font (SIL OFL)."},
        {"about.disclaimer",
         "Unofficial and not affiliated with Action Button Entertainment or Freshuu. No game files are included: "
         "everything the game shows comes from your own copy."},
        // crash / launch
        {"crash.title", "THE GAME CLOSED UNEXPECTEDLY"},
        {"crash.text", "Exit code {0}. Last lines of the log:"},
        {"crash.missing", "The game could not find its files. Please select your game again."},
        {"launch.failed", "Could not start the game: {0}"},
        // dump check messages (zig::DumpMessage ids; args as passed by core/DumpImport.cpp)
        {"msg.dump.compressed", "This is a {0} archive, not the game: extract it first, or select the .ipa inside."},
        {"msg.dump.compressed_in_folder", "No .ipa found, but there are archives here ({0}): extract them first."},
        {"msg.dump.damaged", "This .ipa is damaged or incomplete ({1}). Copy it again from the source."},
        {"msg.dump.not_dump", "\"{0}\" is not a game dump. Select the ZiGGURAT .ipa or its .app folder."},
        {"msg.dump.not_found", "Nothing found at \"{0}\"."},
        {"msg.dump.none_in_folder", "No ZiGGURAT .ipa or .app found in this folder."},
        {"msg.dump.not_app", "This is not an iOS app (no Info.plist found)."},
        {"msg.dump.unreadable", "Can't read the file or folder ({1})."},
        {"msg.dump.bad_info_plist", "The app's Info.plist can't be read."},
        {"msg.dump.not_ziggurat", "This app is not ZiGGURAT (found: {0})."},
        {"msg.dump.incomplete", "Files are missing: {0} ({1}). The dump is incomplete."},
        {"msg.dump.read_errors", "Some files can't be read: {1}. The dump may be damaged."},
        {"msg.dump.modified", "Files were modified: {0} ({1}). Use an unmodified dump."},
        {"msg.dump.verified", "ZiGGURAT {0} recognised ({1}/{2} files OK)."},
        {"msg.dump.unknown_version", "ZiGGURAT {0} is not a known version (expected {1}); {2} files differ. You can import it anyway (unverified)."},
    };
    return m;
}

std::map<std::string, std::string> gTable;
std::string gLang = "en";

std::string systemLanguage() {
    int n = 0;
    std::string l;
    if (SDL_Locale** loc = SDL_GetPreferredLocales(&n)) {
        if (n > 0 && loc[0]->language) {
            l = loc[0]->language;
            if (l == "pt" && loc[0]->country && std::string(loc[0]->country) == "BR") l = "pt-BR";
        }
        SDL_free(loc);
    }
    for (auto& g : languages())
        if (l == g.code) return l;
    return "en";
}

}  // namespace

const std::vector<Lang>& languages() {
    static const std::vector<Lang> v = {{"en", "ENGLISH"}, {"de", "DEUTSCH"}, {"es", "ESPAÑOL"},
                                        {"fr", "FRANÇAIS"}, {"it", "ITALIANO"}, {"pt", "PORTUGUÊS"},
                                        {"pt-BR", "PORTUGUÊS (BRASIL)"}};
    return v;
}

std::string setLanguage(const std::string& code) {
    gLang = (code.empty() || code == "system") ? systemLanguage() : code;
    gTable = english();
    for (const std::string& dir : {zig::Paths::exeDir() + "strings/", zig::Paths::exeDir() + "../src/launcher/strings/"}) {
        std::ifstream in(std::filesystem::u8path(dir + gLang + ".json"));
        if (!in) continue;
        std::stringstream ss;
        ss << in.rdbuf();
        auto j = nlohmann::json::parse(ss.str(), nullptr, false);
        if (j.is_object())
            for (auto& [k, v] : j.items())
                if (v.is_string()) gTable[k] = v.get<std::string>();
        break;
    }
    return gLang;
}

const std::string& language() { return gLang; }

const std::string& T(const char* id) {
    if (gTable.empty()) gTable = english();
    auto it = gTable.find(id);
    if (it != gTable.end()) return it->second;
    static std::map<std::string, std::string> unknown;
    return unknown.emplace(id, id).first->second;
}

bool has(const std::string& id) {
    if (gTable.empty()) gTable = english();
    return gTable.count(id) != 0;
}

std::string Tf(const char* id, const std::vector<std::string>& args) {
    std::string s = T(id);
    for (size_t i = 0; i < args.size(); i++) {
        std::string key = "{" + std::to_string(i) + "}";
        for (size_t p; (p = s.find(key)) != std::string::npos;) s.replace(p, key.size(), args[i]);
    }
    return s;
}

bool writeEnglish(const std::string& file) {
    nlohmann::ordered_json j = nlohmann::ordered_json::object();
    for (auto& [k, v] : english()) j[k] = v;
    std::ofstream out(std::filesystem::u8path(file));
    out << j.dump(2) << '\n';
    return (bool)out;
}

}  // namespace lz
