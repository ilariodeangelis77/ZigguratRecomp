// Developer tool: compiled only with ZIGGURAT_TEST_HOOKS (not in release builds).
#include "core/TestHooks.h"
#if ZIGGURAT_TEST_HOOKS
// Glyph-coverage test (L9): ZigguratLauncher --glyph-test <assetsDir> <report.txt> [sample.bmp]
// Collects every character used by the launcher's strings/*.json and the game's own
// <assetsDir>/strings/*.json, renders each with the real theme font (Flixel.ttf) at both UI sizes and
// reports whether it is native, composed (base + pixel accent), from the fallback font, or missing.
// Exit code: 0 = no Missing glyphs, 1 = something is missing, 2 = setup error.
#include <SDL3/SDL.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include "core/Paths.h"
#include "json.hpp"
#include "launcher/ui/Font.h"
#include "launcher/ui/Gfx.h"
#include "launcher/ui/Theme.h"

namespace fs = std::filesystem;
using namespace zui;

namespace {
void collect(const fs::path& dir, std::map<uint32_t, std::set<std::string>>& used) {
    std::error_code ec;
    for (auto& e : fs::directory_iterator(dir, ec)) {
        if (e.path().extension() != ".json") continue;
        std::ifstream in(e.path());
        std::stringstream ss;
        ss << in.rdbuf();
        auto j = nlohmann::json::parse(ss.str(), nullptr, false);
        if (!j.is_object()) continue;
        std::string tag = e.path().parent_path().filename().u8string() + "/" + e.path().filename().u8string();
        for (auto& [k, v] : j.items()) {
            if (!v.is_string()) continue;
            for (uint32_t cp : utf8Decode(v.get<std::string>()))
                if (cp >= 0x80) used[cp].insert(tag);
        }
    }
}
const char* name(GlyphSource s) {
    switch (s) {
        case GlyphSource::Native: return "native";
        case GlyphSource::Composed: return "composed";
        case GlyphSource::Fallback: return "fallback";
        default: return "MISSING";
    }
}
std::string utf8(uint32_t cp) {
    std::string s;
    if (cp < 0x80) s += (char)cp;
    else if (cp < 0x800) { s += (char)(0xC0 | (cp >> 6)); s += (char)(0x80 | (cp & 63)); }
    else { s += (char)(0xE0 | (cp >> 12)); s += (char)(0x80 | ((cp >> 6) & 63)); s += (char)(0x80 | (cp & 63)); }
    return s;
}
}  // namespace

// Width test (L9): ZigguratLauncher --width-test <assetsDir> <report.txt>
// The bottom help line is ONE line of 460 px and only shows the focused widget's help, so a screenshot
// sweep can't see most of them: measure every "*.help" string of every language with the real font.
// Also checks the tab names fit in one tab bar (466 px). Exit 0 = everything fits.
int runWidthTest(const std::string& assetsDir, const std::string& report) {
    if (!SDL_Init(SDL_INIT_VIDEO)) return 2;
    if (!gfx().init("width test", 1, false)) return 2;
    std::string fb;
    for (std::string d : {zig::Paths::exeDir() + "fonts/", zig::Paths::exeDir() + "../third_party/fonts/"})
        if (zig::fileExists(d + "Silkscreen-Regular.ttf")) { fb = d + "Silkscreen-Regular.ttf"; break; }
    theme().load(assetsDir, fb);
    if (!theme().real()) return 2;
    const Font& f = theme().font();
    fs::path dir;
    for (std::string d : {zig::Paths::exeDir() + "strings", zig::Paths::exeDir() + "../src/launcher/strings"})
        if (zig::dirExists(d)) { dir = fs::u8path(d); break; }
    std::ofstream out(fs::u8path(report));
    int bad = 0;
    std::error_code ec;
    for (auto& e : fs::directory_iterator(dir, ec)) {
        if (e.path().extension() != ".json") continue;
        std::ifstream in(e.path());
        std::stringstream ss;
        ss << in.rdbuf();
        auto j = nlohmann::json::parse(ss.str(), nullptr, false);
        if (!j.is_object()) continue;
        std::string lang = e.path().stem().string();
        int tabs = 0;
        for (const char* t : {"tab.home", "tab.display", "tab.audio", "tab.controls", "tab.language", "tab.game",
                              "tab.data", "tab.steam", "tab.about"})
            if (j.contains(t) && j[t].is_string()) tabs += f.width(j[t].get<std::string>(), Theme::kSmall) + 16;
        if (tabs - 2 > 466) {
            out << lang << ": TABBAR " << tabs - 2 << "px > 466px\n";
            bad++;
        }
        for (auto& [k, v] : j.items()) {
            if (!v.is_string() || k.size() < 5 || k.compare(k.size() - 5, 5, ".help") != 0) continue;
            if (k == "steam.notfound.help") continue;  // drawn as a wrapping text block, not the help line
            int w = f.width(v.get<std::string>(), Theme::kSmall);
            if (w > 460) {
                out << lang << ": " << k << " " << w << "px > 460px: " << v.get<std::string>() << '\n';
                bad++;
            }
        }
    }
    out << "TOTAL TOO WIDE: " << bad << '\n';
    SDL_Log("width test: %d string(s) too wide", bad);
    gfx().shutdown();
    SDL_Quit();
    return bad ? 1 : 0;
}

int runGlyphTest(const std::string& assetsDir, const std::string& report, const std::string& sample) {
    if (!SDL_Init(SDL_INIT_VIDEO)) return 2;
    if (!gfx().init("glyph test", 2, false)) return 2;
    std::string fb;
    for (std::string d : {zig::Paths::exeDir() + "fonts/", zig::Paths::exeDir() + "../third_party/fonts/"})
        if (zig::fileExists(d + "Silkscreen-Regular.ttf")) { fb = d + "Silkscreen-Regular.ttf"; break; }
    theme().load(assetsDir, fb);
    if (!theme().real()) { SDL_Log("glyph test: real theme not available in %s", assetsDir.c_str()); return 2; }
    std::map<uint32_t, std::set<std::string>> used;
    for (std::string d : {zig::Paths::exeDir() + "strings", zig::Paths::exeDir() + "../src/launcher/strings"})
        if (zig::dirExists(d)) { collect(fs::u8path(d), used); break; }
    collect(fs::u8path(assetsDir) / "strings", used);
    // language names shown in the LANGUAGE tab are part of the UI too
    for (uint32_t cp : utf8Decode("ESPAÑOL FRANÇAIS PORTUGUÊS")) if (cp >= 0x80) used[cp].insert("languages");

    const Font& f = theme().font();
    std::ofstream out(fs::u8path(report));
    int missing = 0, composed = 0, fallback = 0, native = 0;
    std::string line;
    for (auto& [cp, where] : used) {
        GlyphSource a = f.source(cp, Theme::kSmall), b = f.source(cp, Theme::kLarge);
        GlyphSource worst = (int)a > (int)b ? a : b;
        switch (worst) {
            case GlyphSource::Native: native++; break;
            case GlyphSource::Composed: composed++; break;
            case GlyphSource::Fallback: fallback++; break;
            default: missing++;
        }
        char buf[64];
        snprintf(buf, sizeof buf, "U+%04X %s  %-8s/%-8s  ", (unsigned)cp, utf8(cp).c_str(), name(a), name(b));
        out << buf;
        for (auto& w : where) out << w << ' ';
        out << '\n';
        line += utf8(cp);
    }
    out << "\nTOTAL " << used.size() << ": native " << native << ", composed " << composed << ", fallback " << fallback
        << ", missing " << missing << '\n';
    out.close();
    SDL_Log("glyph test: %zu chars, native %d composed %d fallback %d missing %d", used.size(), native, composed,
            fallback, missing);
    if (!sample.empty()) {
        gfx().begin();
        gfx().fill({0, 0, 480, 320}, Color(0, 0, 0));
        int y = 8;
        const std::string lines[] = {"ESPAÑOL  FRANÇAIS  PORTUGUÊS  DEUTSCH  ÜBER  ÉCRAN",
                                     "ÀÁÂÃÄ ÇÈÉÊË ÌÍÎÏ ÑÒÓÔÕÖ ÙÚÛÜ",
                                     "àáâãä çèéêë ìíîï ñòóôõö ùúûü ß œ « »"};
        for (float sz : {Theme::kSmall, Theme::kLarge}) {
            for (auto& l : lines) {
                f.draw(l, 8, y, sz, Color(255, 255, 255), Color(0, 0, 0, 255));
                y += f.lineHeight(sz) + 6;
            }
            std::string all;
            for (auto& [cp, w] : used) all += utf8(cp) + " ";
            for (auto& l : f.wrap(all, sz, 464)) {
                f.draw(l, 8, y, sz, Color(255, 255, 0));
                y += f.lineHeight(sz) + 4;
            }
            y += 6;
        }
        gfx().saveCanvasBMP(sample);
    }
    gfx().shutdown();
    SDL_Quit();
    return missing ? 1 : 0;
}

#endif  // ZIGGURAT_TEST_HOOKS
