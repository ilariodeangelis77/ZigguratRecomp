// zigcore: config.json shared by the game and the launcher (schema: LAUNCHER_PLAN.md §9).
// Defaults reproduce the original game. Unknown keys are preserved on save.
#pragma once
#include <map>
#include <string>

namespace zig {

struct GameCfg {
    std::string dumpSource;        // path the user selected (.ipa / .app / folder)
    std::string dumpVersion;       // e.g. "1.0.1"
    bool dumpVerified = false;
    std::string assetsDir;         // empty = Paths::defaultAssetsDir()
    std::string language = "system";   // "system" | en de es fr it pt pt-BR
    std::string controlScheme = "precision";  // mirrors NSUserDefaults preferredControlScheme
    bool aimGuide = false;
    bool showTutorial = false;     // force tutorial at next start
};

struct DisplayCfg {
    std::string mode = "windowed";       // windowed | borderless | exclusive
    std::string monitor;                 // display name, empty = primary
    int windowScale = 2;                 // windowed size = 480x320 * scale
    int customWidth = 0, customHeight = 0;   // >0 overrides windowScale
    std::string fullscreenResolution = "desktop";  // "desktop" or "WxH@Hz"
    bool integerScaling = true;
    std::string filter = "nearest";      // nearest | sharp_bilinear | linear
    std::string aspect = "original";     // original | stretch
    std::string borders = "black";       // black | night_sky | sunset | blur
    bool vsync = true;
    int fps = 30;                        // 30 (frameInterval 2, original iPhone) | 60 (retina)
    bool showFps = false;
};

struct AudioCfg {
    float master = 1.0f, music = 1.0f, sfx = 1.0f;
    std::string device = "default";
    bool muteInBackground = true;
};

// Controller / keyboard mapping (LAUNCHER_PLAN.md §7.4). The game is touch-only: every device
// drives one virtual finger (gameplay) or a menu cursor (menus). Defaults keep the original feel.
struct GyroCfg {
    bool enabled = false;
    float sensitivity = 1.0f;
    std::string mode = "always";     // always | hold (while the gyro_hold binding is held)
    bool invertX = false, invertY = false;
};

struct GamepadCfg {
    std::string guid = "auto";       // "auto" = most recently used pad, else SDL GUID string
    std::string glyphs = "auto";     // auto | xbox | playstation | nintendo
    std::string aimMode = "slide";   // precision scheme: slide (stick moves the finger) | direct (right stick = aim)
    float sensitivity = 1.0f;        // x 240 px/s for slide, pull distance multiplier for slingshot
    float deadzoneLeft = 0.15f, deadzoneRight = 0.15f, outerDeadzone = 0.95f;
    std::string curve = "linear";    // linear | quadratic
    float cursorSpeed = 1.0f;        // menu cursor, x 300 px/s
    float rumble = 0.0f;             // ENHANCEMENT: the original never vibrates -> default off
    GyroCfg gyro;
    // action -> binding name (a b x y back start guide lb rb lt rt lstick rstick dpad_up/down/left/right, "" = none)
    std::map<std::string, std::string> bindings;  // filled by defaults()
    static std::map<std::string, std::string> defaultBindings();
};

struct KeyboardCfg {
    // action -> SDL key name (SDL_GetKeyFromName)
    std::map<std::string, std::string> keys;
    static std::map<std::string, std::string> defaultKeys();
};

struct InputCfg {
    bool mouseAsTouch = true;
    GamepadCfg gamepad;
    KeyboardCfg keyboard;
    InputCfg() { gamepad.bindings = GamepadCfg::defaultBindings(); keyboard.keys = KeyboardCfg::defaultKeys(); }
};

struct LauncherCfg {
    std::string onPlay = "minimize";     // close | minimize | stay
    std::string fromSteam = "show_launcher";  // show_launcher | start_game
    bool music = false;
    int uiTextScale = 1;
    int windowScale = 2;
};

// Enhancements & accessibility (LAUNCHER_PLAN.md §7.9). Every default reproduces the original game.
struct EnhancementsCfg {
    std::string preset = "original";     // original | recommended | custom
    // accessibility
    bool reduceFlashing = false;         // dim white-outs / lightning / red death screen to 30 %
    float screenShake = 1.0f;            // FlxQuake intensity multiplier (0..1)
    // convenience
    bool skipLogos = false;              // StartState: jump past the Action Button / Freshuu / Flixel logos
    bool showDeathSnapshot = false;      // GameOverState: draw the PlayState -snapshot: the original stores but never draws
    // original bugs (default = keep the bug)
    bool fixPurpleDoubleCount = false;   // updateStats adds purpleKills twice
    bool fixYellowAsRed = false;         // yellow aliens killed by toppling aliens count as red kills
    bool fixRedPounce = false;           // red alien pounce timer never reset (changes difficulty!)
    bool showJaOverlay = false;          // OptionsState builds the Japanese alt-text images but never adds them
    // practice (stats are not recorded while any of these is active)
    bool godMode = false;
    int startEnemyLevel = 0;             // 0 = original start
    // developer
    bool devOptions = false;             // launcher: show the developer section
    bool showOverlay = false;            // fps / entity counters
    bool touchZones = false;             // draw the touch zones of the current control scheme
    int fixedSeed = 0;                   // 0 = random (arc4random); else seeds the port's RNG
    std::string logLevel = "info";       // info | debug | verbose
    bool photosensitivityNoticeShown = false;
    bool practiceActive() const { return godMode || startEnemyLevel > 0; }
    void applyPreset(const std::string& p);  // sets fields for "original" / "recommended"
};

// Optional extras (LAUNCHER_PLAN.md §7.6 logo/Steam Input, §7.11 overrides/updates/reference mode).
struct ExtrasCfg {
    bool overrides = true;               // use <data>/overrides/<file> before the imported assets
    std::string logoPosition = "default";  // default | BottomLeft | UpperLeft | CenterCenter | UpperCenter | BottomCenter
    int logoSizePct = 50;                // nWidthPct / nHeightPct (10..100)
    std::string steamInput = "default";  // default | enabled | disabled (localconfig.vdf UseSteamControllerConfig)
    bool updateCheck = false;            // off by default; never downloads anything
    std::string updateFeed;              // empty = no public release channel configured
    std::string touchhlePath;            // developer reference mode; empty = auto-detect
};

struct Config {
    int version = 1;
    GameCfg game;
    DisplayCfg display;
    AudioCfg audio;
    LauncherCfg launcher;
    InputCfg input;
    EnhancementsCfg enhancements;
    ExtrasCfg extras;

    static Config& get();                    // loaded lazily from Paths::configFile()
    bool load(const std::string& file = "");
    bool save(const std::string& file = "") const;   // atomic (tmp + rename)
    std::string assetsDir() const;           // resolved
    void resetSection(const std::string& section);   // "display" / "audio" / ...
    std::string raw;                          // last loaded JSON text (keeps unknown keys)
};

}  // namespace zig
