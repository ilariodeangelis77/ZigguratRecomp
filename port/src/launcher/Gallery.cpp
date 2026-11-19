// Developer tool: compiled only with ZIGGURAT_TEST_HOOKS (not in release builds).
#include "core/TestHooks.h"
#if ZIGGURAT_TEST_HOOKS
// ZigUI widget gallery (L3), reachable with ZigguratLauncher --gallery.
//   ZigguratLauncher.exe [--fallback] [--tab N] [--shot FRAME out.bmp | --shot-ms MS out.bmp] [--fullscreen] [--scale N]
// Keys: T toggles theme (crossfade), M opens a message box, F11 fullscreen, P cycles prompt device.

#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/Config.h"
#include "core/Log.h"
#include "core/Paths.h"
#include "launcher/ui/Audio.h"
#include "launcher/ui/ZigUI.h"

using namespace zui;

namespace {

std::string findAssets() {
    std::vector<std::string> c = {zig::Config::get().assetsDir(), zig::Paths::exeDir() + "assets",
                                  zig::Paths::exeDir() + "../assets", "assets"};
    for (auto& d : c)
        if (zig::fileExists(d + "/img_pillar.png") && zig::fileExists(d + "/Flixel.ttf")) return d;
    return c.front();
}

std::string findFallbackFont() {
    for (std::string d : {zig::Paths::exeDir() + "fonts/", zig::Paths::exeDir() + "../third_party/fonts/",
                          std::string("third_party/fonts/")})
        if (zig::fileExists(d + "Silkscreen-Regular.ttf")) return d + "Silkscreen-Regular.ttf";
    return "";
}

}  // namespace

int runGallery(int argc, char** argv) {
    bool fallback = false, fullscreen = false;
    int tab = 0, shotFrame = -1, scale = 2, shotMs = -1;
    std::string shotFile;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--fallback")) fallback = true;
        else if (!strcmp(argv[i], "--fullscreen")) fullscreen = true;
        else if (!strcmp(argv[i], "--tab") && i + 1 < argc) tab = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--shot") && i + 2 < argc) { shotFrame = atoi(argv[++i]); shotFile = argv[++i]; }
        else if (!strcmp(argv[i], "--shot-ms") && i + 2 < argc) { shotMs = atoi(argv[++i]); shotFile = argv[++i]; }
    }
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) return 1;
    if (!gfx().init("ZiGGURAT Launcher (unofficial)", scale, fullscreen)) return 1;
    audio::init();
    theme().load(findAssets(), findFallbackFont());
    theme().useFallback(fallback || !theme().realAvailable());
    SDL_Log("launcher: real theme %s", theme().realAvailable() ? "available" : "missing");

    // gallery state
    int res = 1, mode = 0, filter = 0, langIdx = 0, sel = 3;
    bool integer = true, vsync = true, music = false;
    float vol = 0.8f, sens = 1.0f, prog = 0.0f;
    bool showBox = false;
    float flash = 0;
    Tex fadeFrom;
    double fadeStart = -10;
    int devCycle = 0;
    std::vector<std::string> listItems = {"ZiGGURAT.ipa  (Downloads)", "Zig2.app  (Desktop)", "Payload/",
                                          "ZiGGURAT-1.0.1.ipa", "backup/ZiGGURAT.ipa", "old_iphone_backup/",
                                          "D:/Games/iOS/", "E:/dumps/zig.ipa", "C:/Users/.../Documents/"};

    double t0 = SDL_GetTicks() / 1000.0, prev = t0;
    for (int frame = 0;; frame++) {
        double now = SDL_GetTicks() / 1000.0 - t0;
        input().beginFrame();
        SDL_Event e;
        while (SDL_PollEvent(&e)) input().handle(e);
        input().endFrame(now);
        if (input().quit) break;
        if (input().key(SDLK_F11)) gfx().toggleFullscreen();
        bool toggleTheme = input().key(SDLK_T);
        if (input().key(SDLK_M)) showBox = true;
        if (input().key(SDLK_P)) {
            devCycle = (devCycle + 1) % 4;
            input().device = (Device)devCycle;
        }
        if (toggleTheme && theme().realAvailable()) {
            fadeFrom = gfx().canvasSnapshot();  // crossfade from the last frame
            fadeStart = now;
            theme().useFallback(theme().real());
        }
        audio::update();
        double dt = now - prev;
        prev = now;

        gfx().begin();
        UI& u = ui();
        u.beginFrame(now);
        theme().background(now);
        // header
        theme().text(theme().real() ? "ZIGUI GALLERY - GAME THEME" : "ZIGUI GALLERY - FALLBACK THEME", 8, 6,
                     Theme::kSmall, pal::textDim);
        u.tabBar("tabs", {8, 18, 300, 16}, {"WIDGETS", "PANELS"}, tab);

        if (tab == 0) {
            Rect p{8, 38, 464, 224};
            u.panel(p, FrameStyle::Big);
            Rect c = theme().frameContent(p, FrameStyle::Big);
            int y = c.y, rh = 16;
            u.selector("res", {c.x, y, c.w, rh}, "RESOLUTION", {"480 x 320 (x1)", "960 x 640 (x2)", "1440 x 960 (x3)", "1920 x 1280 (x4)"},
                       res, "Window size: multiples of the original 480x320 screen.");
            y += rh + 1;
            u.selector("mode", {c.x, y, c.w, rh}, "DISPLAY MODE", {"WINDOWED", "BORDERLESS", "FULLSCREEN"}, mode,
                       "Borderless fullscreen is recommended.");
            y += rh + 1;
            u.toggle("int", {c.x, y, c.w, rh}, "INTEGER SCALING", integer, "Keep pixels perfectly square.");
            y += rh + 1;
            u.selector("filter", {c.x, y, c.w, rh}, "FILTER", {"SHARP PIXELS", "SHARP-SMOOTH", "SMOOTH"}, filter,
                       "How the picture is scaled when integer scaling is off.");
            y += rh + 1;
            u.toggle("vsync", {c.x, y, c.w, rh}, "V-SYNC", vsync, "Synchronise with the display refresh.");
            y += rh + 1;
            u.slider("vol", {c.x, y, c.w, rh}, "MUSIC VOLUME", vol, 0, 1, 0.1f, "Music volume.");
            y += rh + 1;
            u.slider("sens", {c.x, y, c.w, rh}, "AIM SENSITIVITY", sens, 0.2f, 3.0f, 0.1f, "Virtual finger speed.",
                     std::to_string((int)(sens * 10 + 0.5f) / 10) + "." + std::to_string((int)(sens * 10 + 0.5f) % 10) + "x");
            y += rh + 1;
            u.selector("lang", {c.x, y, c.w, rh}, "LANGUAGE", {"SYSTEM", "ENGLISH", "DEUTSCH", "ESPAÑOL", "FRANÇAIS", "ITALIANO", "PORTUGUÊS", "PORTUGUÊS (BR)"},
                       langIdx, "Language of the game and the launcher.");
            y += rh + 1;
            if (u.toggle("music", {c.x, y, c.w, rh}, "LAUNCHER MUSIC", music, "Play the title music in the launcher."))
                audio::music(music ? theme().musicPath() : "");
            y += rh + 6;
            if (u.button("box", {c.x, y, 110, 22}, "MESSAGE BOX", "Open a confirm dialog.")) showBox = true;
            if (u.button("err", {c.x + 118, y, 110, 22}, "ERROR", "Play the error feedback.")) u.error();
            if (u.bigButton("play", {c.x + c.w - 150, y - 4, 150, 30}, "PLAY NOW", "Start ZiGGURAT.")) flash = 1.0f;
        } else {
            Rect a{8, 38, 230, 108};
            u.panel(a, FrameStyle::Small);
            Rect ac = theme().frameContent(a, FrameStyle::Small);
            u.label("GAME DUMP", ac.x, ac.y, Theme::kLarge);
            u.textBlock({ac.x, ac.y + 20, ac.w, 50},
                        "Select your own copy of ZiGGURAT (2012, iOS): an .ipa file or the Zig2.app folder. "
                        "Encrypted App Store dumps work too.", Theme::kSmall, pal::btnBorder);
            prog = (float)std::fmod(now / 6.0, 1.0);
            u.progress({ac.x, ac.y + ac.h - 10, ac.w, 10}, prog, "");
            Rect b{244, 38, 228, 108};
            u.panel(b, FrameStyle::Small);
            Rect bc = theme().frameContent(b, FrameStyle::Small);
            if (u.list("files", bc, listItems, sel, "Pick a file (Enter / A / double-click).")) showBox = true;
            theme().logo(kW / 2, 150);  // ziglogo header (384x85)
            theme().bigArrow(70, 238, false, now);
            theme().bigArrow(341, 238, true, now);
            u.button("btn1", {150, 246, 80, 22}, "BACK", "Small button.");
            u.button("btn2", {250, 246, 80, 22}, "OK!", "Small button.");
        }
        u.helpLine({10, 270, 460, 10});
        u.prompts({8, 290, 464, 12}, {{Act::Ok, "SELECT"}, {Act::Back, "BACK"}, {Act::TabPrev, ""}, {Act::TabNext, "TABS"}, {Act::Y, "DEFAULTS"}});
        theme().text("T theme  M box  P glyphs  F11 fullscreen", 8, 306, Theme::kSmall, pal::textDim);
        if (showBox) {
            int r = u.messageBox("confirm", "ADD TO STEAM?", "Steam must be closed to add ZiGGURAT. Close Steam now and continue?",
                                 {"CLOSE STEAM", "CANCEL"});
            if (r >= 0) showBox = false;
        }
        if (flash > 0) {
            theme().silhouetteFlash(flash);
            flash -= (float)dt * 1.5f;
        }
        // theme crossfade overlay
        if (fadeFrom && now - fadeStart < 0.5) {
            uint8_t a = (uint8_t)(255 * (1.0 - (now - fadeStart) / 0.5));
            gfx().draw(fadeFrom, 0, 0, Color(255, 255, 255, a));
        } else if (fadeFrom) gfx().destroy(fadeFrom);
        u.endFrame();
        if (frame == shotFrame || (shotMs >= 0 && now * 1000.0 >= shotMs)) {
            gfx().saveCanvasBMP(shotFile);
            break;
        }
        gfx().present();
    }
    audio::shutdown();
    gfx().shutdown();
    SDL_Quit();
    return 0;
}

#endif  // ZIGGURAT_TEST_HOOKS
