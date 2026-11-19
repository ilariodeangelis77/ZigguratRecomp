#include "core/Config.h"

#include <SDL3/SDL.h>

#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/Paths.h"
#include "json.hpp"

using nlohmann::json;
namespace fs = std::filesystem;

namespace zig {
namespace {

template <class T>
void rd(const json& j, const char* k, T& v) {
    auto it = j.find(k);
    if (it == j.end()) return;
    try {
        v = it->get<T>();
    } catch (...) {
        SDL_Log("config: bad value for '%s', using default", k);
    }
}

json section(const json& root, const char* name) {
    auto it = root.find(name);
    return (it != root.end() && it->is_object()) ? *it : json::object();
}

}  // namespace

Config& Config::get() {
    static Config c;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        c.load();
    }
    return c;
}

bool Config::load(const std::string& file) {
    std::string path = file.empty() ? Paths::configFile() : file;
    std::ifstream in(fs::u8path(path));
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    raw = ss.str();
    json root = json::parse(raw, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        SDL_Log("config: %s is not valid JSON, using defaults", path.c_str());
        return false;
    }
    rd(root, "version", version);
    json g = section(root, "game");
    rd(g, "dump_source", game.dumpSource);
    rd(g, "dump_version", game.dumpVersion);
    rd(g, "dump_verified", game.dumpVerified);
    rd(g, "assets_dir", game.assetsDir);
    rd(g, "language", game.language);
    rd(g, "control_scheme", game.controlScheme);
    rd(g, "aim_guide", game.aimGuide);
    rd(g, "show_tutorial", game.showTutorial);
    json d = section(root, "display");
    rd(d, "mode", display.mode);
    rd(d, "monitor", display.monitor);
    rd(d, "window_scale", display.windowScale);
    rd(d, "custom_width", display.customWidth);
    rd(d, "custom_height", display.customHeight);
    rd(d, "fullscreen_resolution", display.fullscreenResolution);
    rd(d, "integer_scaling", display.integerScaling);
    rd(d, "filter", display.filter);
    rd(d, "aspect", display.aspect);
    rd(d, "borders", display.borders);
    rd(d, "vsync", display.vsync);
    rd(d, "fps", display.fps);
    rd(d, "show_fps", display.showFps);
    json a = section(root, "audio");
    rd(a, "master", audio.master);
    rd(a, "music", audio.music);
    rd(a, "sfx", audio.sfx);
    rd(a, "device", audio.device);
    rd(a, "mute_in_background", audio.muteInBackground);
    json l = section(root, "launcher");
    rd(l, "on_play", launcher.onPlay);
    rd(l, "from_steam", launcher.fromSteam);
    rd(l, "music", launcher.music);
    rd(l, "ui_text_scale", launcher.uiTextScale);
    rd(l, "window_scale", launcher.windowScale);
    json ij = section(root, "input");
    rd(ij, "mouse_as_touch", input.mouseAsTouch);
    json gp = section(ij, "gamepad");
    auto& G = input.gamepad;
    rd(gp, "guid", G.guid);
    rd(gp, "glyphs", G.glyphs);
    rd(gp, "aim_mode", G.aimMode);
    rd(gp, "sensitivity", G.sensitivity);
    rd(gp, "deadzone_left", G.deadzoneLeft);
    rd(gp, "deadzone_right", G.deadzoneRight);
    rd(gp, "outer_deadzone", G.outerDeadzone);
    rd(gp, "curve", G.curve);
    rd(gp, "cursor_speed", G.cursorSpeed);
    rd(gp, "rumble", G.rumble);
    json gy = section(gp, "gyro");
    rd(gy, "enabled", G.gyro.enabled);
    rd(gy, "sensitivity", G.gyro.sensitivity);
    rd(gy, "mode", G.gyro.mode);
    rd(gy, "invert_x", G.gyro.invertX);
    rd(gy, "invert_y", G.gyro.invertY);
    json gb = section(gp, "bindings");
    for (auto& [k, v] : gb.items()) if (v.is_string()) G.bindings[k] = v.get<std::string>();
    json kb = section(section(ij, "keyboard"), "keys");
    for (auto& [k, v] : kb.items()) if (v.is_string()) input.keyboard.keys[k] = v.get<std::string>();
    {
        json e = section(root, "enhancements");
        auto& E = enhancements;
        rd(e, "preset", E.preset);
        rd(e, "reduce_flashing", E.reduceFlashing);
        rd(e, "screen_shake", E.screenShake);
        rd(e, "skip_logos", E.skipLogos);
        rd(e, "show_death_snapshot", E.showDeathSnapshot);
        rd(e, "fix_purple_double_count", E.fixPurpleDoubleCount);
        rd(e, "fix_yellow_as_red", E.fixYellowAsRed);
        rd(e, "fix_red_pounce", E.fixRedPounce);
        rd(e, "show_ja_overlay", E.showJaOverlay);
        rd(e, "god_mode", E.godMode);
        rd(e, "start_enemy_level", E.startEnemyLevel);
        rd(e, "dev_options", E.devOptions);
        rd(e, "show_overlay", E.showOverlay);
        rd(e, "touch_zones", E.touchZones);
        rd(e, "fixed_seed", E.fixedSeed);
        rd(e, "log_level", E.logLevel);
        rd(e, "photosensitivity_notice_shown", E.photosensitivityNoticeShown);
        if (E.screenShake < 0) E.screenShake = 0;
        if (E.screenShake > 1) E.screenShake = 1;
        if (E.startEnemyLevel < 0) E.startEnemyLevel = 0;
    }
    {
        json x = section(root, "extras");
        auto& X = extras;
        rd(x, "overrides", X.overrides);
        rd(x, "logo_position", X.logoPosition);
        rd(x, "logo_size_pct", X.logoSizePct);
        rd(x, "steam_input", X.steamInput);
        rd(x, "update_check", X.updateCheck);
        rd(x, "update_feed", X.updateFeed);
        rd(x, "touchhle_path", X.touchhlePath);
        if (X.logoSizePct < 10) X.logoSizePct = 10;
        if (X.logoSizePct > 100) X.logoSizePct = 100;
    }
    // sanitize
    if (display.windowScale < 1) display.windowScale = 1;
    if (display.fps != 30 && display.fps != 60) display.fps = 30;
    return true;
}

bool Config::save(const std::string& file) const {
    std::string path = file.empty() ? Paths::configFile() : file;
    json root = json::parse(raw.empty() ? "{}" : raw, nullptr, false);
    if (root.is_discarded() || !root.is_object()) root = json::object();
    root["version"] = version;
    auto& g = root["game"];
    if (!g.is_object()) g = json::object();
    g["dump_source"] = game.dumpSource;
    g["dump_version"] = game.dumpVersion;
    g["dump_verified"] = game.dumpVerified;
    g["assets_dir"] = game.assetsDir;
    g["language"] = game.language;
    g["control_scheme"] = game.controlScheme;
    g["aim_guide"] = game.aimGuide;
    g["show_tutorial"] = game.showTutorial;
    auto& d = root["display"];
    if (!d.is_object()) d = json::object();
    d["mode"] = display.mode;
    d["monitor"] = display.monitor;
    d["window_scale"] = display.windowScale;
    d["custom_width"] = display.customWidth;
    d["custom_height"] = display.customHeight;
    d["fullscreen_resolution"] = display.fullscreenResolution;
    d["integer_scaling"] = display.integerScaling;
    d["filter"] = display.filter;
    d["aspect"] = display.aspect;
    d["borders"] = display.borders;
    d["vsync"] = display.vsync;
    d["fps"] = display.fps;
    d["show_fps"] = display.showFps;
    auto& a = root["audio"];
    if (!a.is_object()) a = json::object();
    a["master"] = audio.master;
    a["music"] = audio.music;
    a["sfx"] = audio.sfx;
    a["device"] = audio.device;
    a["mute_in_background"] = audio.muteInBackground;
    auto& l = root["launcher"];
    if (!l.is_object()) l = json::object();
    l["on_play"] = launcher.onPlay;
    l["from_steam"] = launcher.fromSteam;
    l["music"] = launcher.music;
    l["ui_text_scale"] = launcher.uiTextScale;
    l["window_scale"] = launcher.windowScale;
    {
        json in = json::object();
        in["mouse_as_touch"] = input.mouseAsTouch;
        const auto& G = input.gamepad;
        json gp = {{"guid", G.guid}, {"glyphs", G.glyphs}, {"aim_mode", G.aimMode}, {"sensitivity", G.sensitivity},
                   {"deadzone_left", G.deadzoneLeft}, {"deadzone_right", G.deadzoneRight},
                   {"outer_deadzone", G.outerDeadzone}, {"curve", G.curve}, {"cursor_speed", G.cursorSpeed},
                   {"rumble", G.rumble}};
        gp["gyro"] = {{"enabled", G.gyro.enabled}, {"sensitivity", G.gyro.sensitivity}, {"mode", G.gyro.mode},
                      {"invert_x", G.gyro.invertX}, {"invert_y", G.gyro.invertY}};
        gp["bindings"] = G.bindings;
        in["gamepad"] = gp;
        in["keyboard"] = {{"keys", input.keyboard.keys}};
        root["input"] = in;
    }
    {
        const auto& E = enhancements;
        root["enhancements"] = {{"preset", E.preset}, {"reduce_flashing", E.reduceFlashing},
                                {"screen_shake", E.screenShake}, {"skip_logos", E.skipLogos},
                                {"show_death_snapshot", E.showDeathSnapshot},
                                {"fix_purple_double_count", E.fixPurpleDoubleCount},
                                {"fix_yellow_as_red", E.fixYellowAsRed}, {"fix_red_pounce", E.fixRedPounce},
                                {"show_ja_overlay", E.showJaOverlay}, {"god_mode", E.godMode},
                                {"start_enemy_level", E.startEnemyLevel}, {"dev_options", E.devOptions},
                                {"show_overlay", E.showOverlay}, {"touch_zones", E.touchZones},
                                {"fixed_seed", E.fixedSeed}, {"log_level", E.logLevel},
                                {"photosensitivity_notice_shown", E.photosensitivityNoticeShown}};
    }
    {
        const auto& X = extras;
        root["extras"] = {{"overrides", X.overrides}, {"logo_position", X.logoPosition},
                          {"logo_size_pct", X.logoSizePct}, {"steam_input", X.steamInput},
                          {"update_check", X.updateCheck}, {"update_feed", X.updateFeed},
                          {"touchhle_path", X.touchhlePath}};
    }

    std::string tmp = path + ".tmp";
    {
        std::ofstream out(fs::u8path(tmp), std::ios::trunc);
        if (!out) return false;
        out << root.dump(2) << '\n';
        if (!out) return false;
    }
    std::error_code ec;
    fs::rename(fs::u8path(tmp), fs::u8path(path), ec);
    if (ec) {
        SDL_Log("config: could not write %s: %s", path.c_str(), ec.message().c_str());
        return false;
    }
    const_cast<Config*>(this)->raw = root.dump(2);
    return true;
}

std::string Config::assetsDir() const {
    return game.assetsDir.empty() ? Paths::defaultAssetsDir() : game.assetsDir;
}

void Config::resetSection(const std::string& s) {
    if (s == "display") display = DisplayCfg{};
    else if (s == "audio") audio = AudioCfg{};
    else if (s == "launcher") launcher = LauncherCfg{};
    else if (s == "input") input = InputCfg{};
    else if (s == "extras") extras = ExtrasCfg{};
    else if (s == "enhancements") {
        bool shown = enhancements.photosensitivityNoticeShown, dev = enhancements.devOptions;
        enhancements = EnhancementsCfg{};
        enhancements.photosensitivityNoticeShown = shown;
        enhancements.devOptions = dev;
    }
    else if (s == "game") {
        GameCfg keep = game;
        game = GameCfg{};
        game.dumpSource = keep.dumpSource;
        game.dumpVersion = keep.dumpVersion;
        game.dumpVerified = keep.dumpVerified;
        game.assetsDir = keep.assetsDir;
    }
}

}  // namespace zig

namespace zig {
void EnhancementsCfg::applyPreset(const std::string& p) {
    // Presets touch accessibility, stat bug fixes and convenience. Practice/developer settings are
    // never changed by a preset. "recommended" deliberately leaves fixRedPounce off (changes difficulty).
    preset = p;
    if (p == "original") {
        reduceFlashing = false;
        screenShake = 1.0f;
        skipLogos = false;
        showDeathSnapshot = false;
        fixPurpleDoubleCount = false;
        fixYellowAsRed = false;
        fixRedPounce = false;
        showJaOverlay = false;
    } else if (p == "recommended") {
        reduceFlashing = true;
        screenShake = 0.5f;
        skipLogos = false;
        showDeathSnapshot = false;
        fixPurpleDoubleCount = true;
        fixYellowAsRed = true;
        fixRedPounce = false;
        showJaOverlay = true;
    }
}

std::map<std::string, std::string> GamepadCfg::defaultBindings() {
    return {{"press", "rt"},          // finger down (hold to charge, release to fire)
            {"press_alt", "a"},       // second finger-down button
            {"menu_tap", "a"},        // tap at the menu cursor
            {"recenter", "rstick"},   // precision: aim straight up again
            {"gyro_hold", "lb"}};     // gyro mode "hold"
}
std::map<std::string, std::string> KeyboardCfg::defaultKeys() {
    return {{"press", "Space"}, {"left", "A"}, {"right", "D"}, {"up", "W"}, {"down", "S"}, {"recenter", "R"}};
}
}  // namespace zig
