#include "launcher/App.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>

#include "core/Log.h"
#include "core/Version.h"
#include "core/ArtworkGen.h"
#include "core/Paths.h"
#include "core/Shortcuts.h"
#include "launcher/Strings.h"
#include "launcher/ui/Audio.h"

namespace fs = std::filesystem;
using namespace zui;
using lz::T;
using lz::Tf;

namespace zl {
int gStartPage = -1;
namespace {

constexpr int kRowH = 14;   // settings row height
constexpr int kRowGap = 1;

std::string fmt1(float v) {
    char b[32];
    snprintf(b, sizeof b, "%.1f", v);
    return b;
}
std::string pct(float v) { return std::to_string((int)std::lround(v * 100)) + "%"; }

int indexOf(const std::vector<std::string>& v, const std::string& s, int def = 0) {
    for (size_t i = 0; i < v.size(); i++)
        if (v[i] == s) return (int)i;
    return def;
}

std::string upper(std::string s) {
    for (auto& c : s) c = (char)toupper((unsigned char)c);
    return s;
}

}  // namespace

// ------------------------------------------------------------------ setup / loop

int App::run(const Options& o) {
    opt_ = o;
    if (!o.steamRoot.empty()) zig::Steam::setRootOverride(o.steamRoot);
    lz::setLanguage(cfg_.game.language);

    bool deck = SDL_getenv("SteamDeck") || SDL_getenv("SteamGamepadUI");
    int scale = o.scale > 0 ? o.scale : cfg_.launcher.windowScale;
    if (!gfx().init("ZiGGURAT Launcher (unofficial)", scale, o.fullscreen || deck)) return 1;
    audio::init();
    audio::setVolumes(cfg_.audio.master, cfg_.audio.music, cfg_.audio.sfx);
    for (std::string d : {zig::Paths::exeDir() + "fonts/", zig::Paths::exeDir() + "../third_party/fonts/",
                          std::string("third_party/fonts/")})
        if (zig::fileExists(d + "Silkscreen-Regular.ttf")) { fallbackFont_ = d + "Silkscreen-Regular.ttf"; break; }
    reloadTheme(false);
    if (o.fallbackTheme) theme().useFallback(true);
    loadStats();

    if (!assetsReady()) enterWizard(Wiz::Welcome);
    else if (o.reimport) enterWizard(Wiz::Locate);
    else screen_ = Screen::Main;
    if (o.startTab >= 0 && o.startTab < TabCount) tab_ = o.startTab;
    gStartPage = o.startPage;
    if (tab_ == Steam) steamRefresh(true);
    if (!o.wizardDump.empty() && screen_ == Screen::Wizard) {
        wiz_ = Wiz::Locate;
        startScan(o.wizardDump);
    }
    bool autoPlay = assetsReady() && (o.play || (o.fromSteam && cfg_.launcher.fromSteam == "start_game"));
    if (autoPlay) play();
    if (cfg_.launcher.music) audio::music(theme().musicPath(), 0.5f);

    double t0 = SDL_GetTicks() / 1000.0;
    size_t shotIdx = 0;
    while (!quit_) {
        now_ = SDL_GetTicks() / 1000.0 - t0;
        input().beginFrame();
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_DROP_FILE && e.drop.data && screen_ == Screen::Wizard &&
                (wiz_ == Wiz::Locate || wiz_ == Wiz::Result)) {
                startScan(e.drop.data);
            }
            input().handle(e);
        }
        input().endFrame(now_);
        if (input().quit) break;
        if (input().key(SDLK_F11)) gfx().toggleFullscreen();
        {   // native dialog results arrive on another thread
            std::lock_guard<std::mutex> lk(dialogMutex_);
            if (!dialogResult_.empty()) {
                std::string p = dialogResult_;
                dialogResult_.clear();
                startScan(p);
            }
        }
        if (musicStopAt_ > 0 && now_ > musicStopAt_) {
            musicStopAt_ = 0;
            audio::music(cfg_.launcher.music ? theme().musicPath() : "");
        }
        audio::update();
        pollGame();

        gfx().begin();
        UI& u = ui();
        u.beginFrame(now_);
        if (!pendingFocus_.empty()) { u.focus(pendingFocus_); pendingFocus_.clear(); }
        theme().background(now_);
        if (screen_ == Screen::Wizard) wizardFrame();
        else mainFrame();
        overlays();
        if (fadeFrom_ && now_ - fadeStart_ < 0.6) {
            uint8_t a = (uint8_t)(255 * (1.0 - (now_ - fadeStart_) / 0.6));
            gfx().draw(fadeFrom_, 0, 0, Color(255, 255, 255, a));
        } else if (fadeFrom_) gfx().destroy(fadeFrom_);
        u.endFrame();

        if (shotIdx < opt_.shots.size() && now_ * 1000.0 >= opt_.shots[shotIdx].first) {
            gfx().saveCanvasBMP(opt_.shots[shotIdx].second);
            if (++shotIdx == opt_.shots.size()) break;
        }
        gfx().present();
    }
    job_.cancel = true;
    job_.join();
    audio::shutdown();
    gfx().shutdown();
    return 0;
}

bool App::assetsReady() const {
    std::string d = cfg_.assetsDir();
    return zig::fileExists(d + "/ziglogo.png") && zig::fileExists(d + "/Flixel.ttf") &&
           zig::fileExists(d + "/img_pillar.png");
}

void App::reloadTheme(bool crossfade) {
    if (crossfade) {
        if (fadeFrom_) gfx().destroy(fadeFrom_);
        fadeFrom_ = gfx().canvasSnapshot();
        fadeStart_ = now_;
    }
    theme().load(assetsReady() ? cfg_.assetsDir() : std::string(), fallbackFont_);
    theme().useFallback(!theme().realAvailable());
}

void App::saveConfig() {
    if (!cfg_.save()) SDL_Log("launcher: could not save config");
}

void App::loadStats() {
    games_ = bestKills_ = totalKills_ = 0;
    bestSeconds_ = 0;
    std::ifstream in(fs::u8path(zig::Paths::data("statistics.txt")));
    std::string line;
    while (std::getline(in, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq);
        double v = atof(line.c_str() + eq + 1);
        if (k == "gamesPlayed") games_ = (int)v;
        else if (k == "bestKills") bestKills_ = (int)v;
        else if (k == "kills") totalKills_ = (int)v;
        else if (k == "bestSeconds") bestSeconds_ = (float)v;
    }
}

std::string App::msgText(const zig::DumpMessage& m) const {
    std::string id = "msg." + m.id;
    if (lz::has(id)) return Tf(id.c_str(), m.args);
    return m.text;
}

void App::openFolder(const std::string& path) {
    zig::makeDirs(path);
    std::string p = path;
    for (auto& c : p) if (c == '\\') c = '/';
    std::string raw = p.size() && p[0] == '/' ? p.substr(1) : p;
    // Percent-encode everything except unreserved characters and path separators, so paths with
    // spaces, accents or CJK characters (UTF-8 bytes) form a valid file:// URL.
    std::string enc;
    static const char* hex = "0123456789ABCDEF";
    for (unsigned char ch : raw) {
        if (isalnum(ch) || ch == '/' || ch == ':' || ch == '-' || ch == '_' || ch == '.' || ch == '~') enc += (char)ch;
        else { enc += '%'; enc += hex[ch >> 4]; enc += hex[ch & 15]; }
    }
    std::string url = "file:///" + enc;
    if (!SDL_OpenURL(url.c_str())) SDL_Log("launcher: open %s failed: %s", url.c_str(), SDL_GetError());
}

// ------------------------------------------------------------------ game launch

void App::play(const std::vector<std::string>& extra) {
    if (game_.running()) return;
    saveConfig();
    std::string err;
    if (!game_.start(extra, err)) {
        toast_ = Tf("launch.failed", {err});
        toastUntil_ = now_ + 4;
        ui().error();
        return;
    }
    theme().silhouetteFlash(1.0f);
    audio::music("");
    if (!previewRunning_) {
        if (cfg_.launcher.onPlay == "close") quit_ = true;
        else if (cfg_.launcher.onPlay == "minimize") SDL_MinimizeWindow(gfx().window());
    }
}

void App::startPreview() {
    displayBeforeTest_ = zig::Config::get().display;
    std::string preview = zig::Paths::data("preview.json");
    zig::Config tmp = cfg_;
    tmp.save(preview);
    previewRunning_ = true;
    play({"--config", preview, "--preview-seconds", "5"});
    if (!game_.running()) previewRunning_ = false;
}

void App::pollGame() {
    if (!game_.finished()) return;
    int code = game_.exitCode();
    SDL_RestoreWindow(gfx().window());
    SDL_RaiseWindow(gfx().window());
    loadStats();
    cfg_.load();  // the game writes back in-game option changes
    if (previewRunning_) {
        previewRunning_ = false;
        askKeep_ = true;
        setFocus("keep#b0");
    }
    if (code == 2) {
        toast_ = T("crash.missing");
        toastUntil_ = now_ + 6;
        enterWizard(Wiz::Locate);
    } else if (code != 0) {
        crash_ = true;
        crashCode_ = code;
        crashLines_ = zig::Log::tail(newestGameLog(), 30);
    }
    if (cfg_.launcher.music) audio::music(theme().musicPath(), 0.5f);
}

// ------------------------------------------------------------------ wizard

void App::enterWizard(Wiz step) {
    screen_ = Screen::Wizard;
    wiz_ = step;
    ui().focus("");
}

void App::startScan(const std::string& path) {
    if (job_.running) return;
    job_.join();
    dumpPath_ = path;
    wiz_ = Wiz::Checking;
    job_.done = false;
    job_.cancel = false;
    job_.cur = 0;
    job_.total = 0;
    job_.running = true;
    job_.th = std::thread([this, path] {
        scan_ = zig::scanForDumps(path);
        if (!scan_.candidates.empty()) {
            verify_ = zig::verifyDump(scan_.candidates.front(), [this](int d, int t, const std::string& it) {
                job_.cur = d;
                job_.total = t;
                std::lock_guard<std::mutex> lk(job_.m);
                job_.item = it;
            });
        } else {
            verify_ = {};
            verify_.status = scan_.status;
            verify_.message = scan_.message;
        }
        job_.running = false;
        job_.done = true;
    });
}

void App::startImport(bool allowUnverified) {
    if (job_.running || scan_.candidates.empty()) return;
    job_.join();
    wiz_ = Wiz::Importing;
    job_.done = false;
    job_.cancel = false;
    job_.cur = 0;
    job_.total = 0;
    job_.running = true;
    std::string dest = cfg_.game.assetsDir.empty() ? zig::Paths::defaultAssetsDir() : cfg_.game.assetsDir;
    job_.th = std::thread([this, dest, allowUnverified] {
        import_ = zig::importDump(scan_.candidates.front(), dest,
                                  [this](int d, int t, const std::string& it) {
                                      job_.cur = d;
                                      job_.total = t;
                                      std::lock_guard<std::mutex> lk(job_.m);
                                      job_.item = it;
                                  },
                                  &job_.cancel, allowUnverified);
        job_.running = false;
        job_.done = true;
    });
}

void App::onImported() {
    cfg_.game.dumpSource = dumpPath_;
    cfg_.game.dumpVersion = verify_.version;
    cfg_.game.dumpVerified = verify_.status == zig::DumpStatus::Verified;
    saveConfig();
    reloadTheme(true);
    loadStats();
    wiz_ = Wiz::Setup;
    setupPreset_ = cfg_.display.mode == "borderless" ? 2 : (cfg_.display.windowScale >= 3 ? 1 : 0);
    setFocus("setup.next");
}

void App::wizardFrame() {
    UI& u = ui();
    Rect p{8, 22, 464, 246};
    theme().text(T("app.title"), 8, 6, Theme::kSmall, pal::textDim);
    if (browser_.isOpen()) {
        if (browser_.frame()) startScan(browser_.result());
        footer(false);
        return;
    }
    u.panel(p, FrameStyle::Big);
    Rect c = theme().frameContent(p, FrameStyle::Big);
    auto title = [&](const char* id) { theme().text(T(id), c.x, c.y, Theme::kLarge, pal::text); };
    int by = c.y + c.h - 22;  // button row

    switch (wiz_) {
        case Wiz::Welcome: {
            title("wiz.welcome.title");
            u.textBlock({c.x, c.y + 24, c.w, 70}, T("wiz.welcome.text"), Theme::kSmall, pal::btnBorder);
            std::vector<std::string> langs{T("lang.system")};
            for (auto& l : lz::languages()) langs.push_back(l.name);
            int li = 0;
            for (size_t i = 0; i < lz::languages().size(); i++)
                if (cfg_.game.language == lz::languages()[i].code) li = (int)i + 1;
            if (u.selector("wiz.lang", {c.x, c.y + 104, c.w, kRowH}, T("wiz.language"), langs, li, T("lang.help"))) {
                cfg_.game.language = li == 0 ? "system" : lz::languages()[li - 1].code;
                lz::setLanguage(cfg_.game.language);
                saveConfig();
            }
            if (u.focused().empty()) u.focus("wiz.next");
            if (u.button("wiz.next", {c.x + c.w - 110, by, 110, 22}, T("btn.next"))) {
                wiz_ = Wiz::Locate;
                setFocus("wiz.ipa");
            }
            break;
        }
        case Wiz::Locate:
        case Wiz::Checking:
        case Wiz::Result: {
            title("wiz.locate.title");
            u.textBlock({c.x, c.y + 22, c.w, 40}, T("wiz.locate.text"), Theme::kSmall, pal::btnBorder);
            int y = c.y + 62;
            if (wiz_ == Wiz::Locate) {
                if (u.focused().empty()) u.focus("wiz.ipa");
                if (u.button("wiz.ipa", {c.x, y, 200, 22}, T("wiz.choose_ipa"), T("wiz.choose_ipa.help"))) {
                    static const SDL_DialogFileFilter filters[] = {{"ZiGGURAT (.ipa)", "ipa"}, {"All files", "*"}};
                    SDL_ShowOpenFileDialog(
                        [](void* ud, const char* const* list, int) {
                            auto* self = (App*)ud;
                            if (list && list[0]) {
                                std::lock_guard<std::mutex> lk(self->dialogMutex_);
                                self->dialogResult_ = list[0];
                            }
                        },
                        this, gfx().window(), filters, 2, nullptr, false);
                }
                if (u.button("wiz.folder", {c.x, y + 26, 200, 22}, T("wiz.choose_folder"), T("wiz.choose_folder.help"))) {
                    SDL_ShowOpenFolderDialog(
                        [](void* ud, const char* const* list, int) {
                            auto* self = (App*)ud;
                            if (list && list[0]) {
                                std::lock_guard<std::mutex> lk(self->dialogMutex_);
                                self->dialogResult_ = list[0];
                            }
                        },
                        this, gfx().window(), nullptr, false);
                }
                if (u.button("wiz.browse", {c.x, y + 52, 200, 22}, T("wiz.browse"), T("wiz.browse.help"))) {
                    browser_.open(cfg_.game.dumpSource.empty() ? "" : fs::u8path(cfg_.game.dumpSource).parent_path().u8string());
                    setFocus("fb.list");
                }
                if (u.button("wiz.back", {c.x + c.w - 110, by, 110, 22}, T("btn.back")) || u.back()) {
                    if (assetsReady()) screen_ = Screen::Main;
                    else wiz_ = Wiz::Welcome;
                }
            } else if (wiz_ == Wiz::Checking) {
                theme().text(T("wiz.scanning"), c.x, y + 8, Theme::kSmall, pal::emphasis);
                int t = job_.total, d = job_.cur;
                u.progress({c.x, y + 26, c.w, 10}, t > 0 ? (float)d / t : 0.0f);
                std::string item;
                { std::lock_guard<std::mutex> lk(job_.m); item = job_.item; }
                theme().text(item, c.x, y + 42, Theme::kSmall, pal::textDim);
                if (job_.done) {
                    job_.join();
                    job_.done = false;
                    if (verify_.status == zig::DumpStatus::Verified) startImport(false);
                    else { wiz_ = Wiz::Result; setFocus(verify_.status == zig::DumpStatus::UnknownVersion ? "wiz.anyway" : "wiz.other"); }
                }
            } else {  // Result: a problem was found
                bool unknown = verify_.status == zig::DumpStatus::UnknownVersion;
                theme().text(upper(zig::toString(verify_.status)), c.x, y, Theme::kSmall, unknown ? pal::emphasis : pal::select);
                u.textBlock({c.x, y + 14, c.w, 60}, msgText(verify_.message), Theme::kSmall, pal::text);
                std::string shown = dumpPath_;
                while (theme().font().width(shown, Theme::kSmall) > c.w && shown.size() > 4) shown = "..." + shown.substr(4);
                theme().text(shown, c.x, y + 78, Theme::kSmall, pal::textDim);
                if (unknown && u.button("wiz.anyway", {c.x, by, 150, 22}, T("wiz.import_anyway"))) startImport(true);
                if (u.button("wiz.other", {c.x + c.w - 150, by, 150, 22}, T("wiz.choose_other")) || u.back()) {
                    wiz_ = Wiz::Locate;
                    setFocus("wiz.ipa");
                }
            }
            break;
        }
        case Wiz::Importing: {
            title("wiz.import.title");
            theme().text(T("wiz.import.text"), c.x, c.y + 26, Theme::kSmall, pal::btnBorder);
            int t = job_.total, d = job_.cur;
            u.progress({c.x, c.y + 50, c.w, 10}, t > 0 ? (float)d / t : 0.0f);
            std::string item;
            { std::lock_guard<std::mutex> lk(job_.m); item = job_.item; }
            theme().text(item, c.x, c.y + 66, Theme::kSmall, pal::textDim);
            if (u.focused().empty()) u.focus("imp.cancel");
            if (u.button("imp.cancel", {c.x + c.w - 110, by, 110, 22}, T("btn.cancel")) || u.back()) job_.cancel = true;
            if (job_.done) {
                job_.join();
                job_.done = false;
                if (import_.ok) onImported();
                else {
                    verify_.status = zig::DumpStatus::Unreadable;
                    verify_.message = import_.message;
                    wiz_ = Wiz::Result;
                    setFocus("wiz.other");
                }
            }
            break;
        }
        case Wiz::Setup: {
            title("wiz.setup.title");
            theme().text(Tf("wiz.import.done", {std::to_string(import_.pngs + import_.audio + import_.fonts + import_.strings + import_.other)}),
                         c.x, c.y + 22, Theme::kSmall, pal::ok);
            int y = c.y + 44;
            std::vector<std::string> presets{T("preset.win2"), T("preset.win3"), T("preset.borderless")};
            if (u.selector("setup.display", {c.x, y, c.w, kRowH}, T("wiz.setup.display"), presets, setupPreset_,
                           T("wiz.setup.display.help"))) {
                if (setupPreset_ == 2) cfg_.display.mode = "borderless";
                else { cfg_.display.mode = "windowed"; cfg_.display.windowScale = setupPreset_ == 0 ? 2 : 3; }
                saveConfig();
            }
            std::vector<std::string> langs{T("lang.system")};
            for (auto& l : lz::languages()) langs.push_back(l.name);
            int li = 0;
            for (size_t i = 0; i < lz::languages().size(); i++)
                if (cfg_.game.language == lz::languages()[i].code) li = (int)i + 1;
            if (u.selector("setup.lang", {c.x, y + kRowH + kRowGap, c.w, kRowH}, T("wiz.language"), langs, li, T("lang.help"))) {
                cfg_.game.language = li == 0 ? "system" : lz::languages()[li - 1].code;
                lz::setLanguage(cfg_.game.language);
                saveConfig();
            }
            if (u.button("setup.next", {c.x + c.w - 110, by, 110, 22}, T("btn.next"))) {
                steamRefresh(false);
                if (steam_.found && !steam_.accounts.empty() && !steamEntry_.added) {
                    wiz_ = Wiz::SteamAsk;
                    setFocus("wsteam.add");
                } else {
                    wiz_ = Wiz::Done;
                    setFocus("done.play");
                }
            }
            break;
        }
        case Wiz::SteamAsk: {
            title("wiz.steam.title");
            u.textBlock({c.x, c.y + 22, c.w, 60}, T("wiz.steam.text"), Theme::kSmall, pal::text);
            if (steam_.running)
                u.textBlock({c.x, c.y + 84, c.w, 30}, T("steam.running.note"), Theme::kSmall, pal::select);
            if (u.button("wsteam.add", {c.x, by, 150, 22}, T("steam.add"), T("steam.add.help"))) steamAdd();
            if (u.button("wsteam.skip", {c.x + c.w - 110, by, 110, 22}, T("wiz.steam.skip")) || u.back()) {
                wiz_ = Wiz::Done;
                setFocus("done.play");
            }
            if (steamEntry_.added) {
                wiz_ = Wiz::Done;
                setFocus("done.play");
            }
            break;
        }
        case Wiz::Done: {
            title("wiz.done.title");
            theme().logo(kW / 2, c.y + 30);
            theme().text(T("wiz.done.text"), c.x, c.y + 124, Theme::kSmall, pal::text, Align::Center, c.w);
            if (u.bigButton("done.play", {c.x + c.w / 2 - 80, by - 8, 160, 30}, T("btn.play"), T("home.play.help"))) {
                screen_ = Screen::Main;
                tab_ = Home;
                play();
            }
            if (u.back()) { screen_ = Screen::Main; tab_ = Home; }
            break;
        }
    }
    footer(false);
}

// ------------------------------------------------------------------ main screen

void App::mainFrame() {
    UI& u = ui();
    std::vector<std::string> tabs{T("tab.home"), T("tab.display"), T("tab.audio"), T("tab.controls"),
                                  T("tab.language"), T("tab.game"), T("tab.data"), T("tab.steam"), T("tab.about")};
    int old = tab_;
    u.tabBar("tabs", {8, 4, 464, 14}, tabs, tab_);
    if (tab_ != old) {
        if (tab_ == Display) refreshDisplayLists();
        if (tab_ == Audio) refreshAudioDevices();
        if (tab_ == Steam) steamRefresh(true);
    }
    Rect p{8, 22, 464, 246};
    u.panel(p, FrameStyle::Big);
    Rect c = theme().frameContent(p, FrameStyle::Big);
    switch (tab_) {
        case Home: tabHome(c); break;
        case Display: tabDisplay(c); break;
        case Audio: tabAudio(c); break;
        case Controls: tabControls(c); break;
        case Language: tabLanguage(c); break;
        case Game: tabGame(c); break;
        case Data: tabData(c); break;
        case Steam: tabSteam(c); break;
        case About: tabAbout(c); break;
    }
    // L8: one-time photosensitivity notice on the first launcher start (drawn last = on top; stored in config.json)
    if (enhTab_.photosensitivityNotice(cfg_.enhancements)) saveConfig();
    // Start = PLAY from anywhere (not while a dialog is open)
    if (!askKeep_ && !crash_ && !confirmRemove_ && !dataTab_.busy() && !enhTab_.noticeOpen() && input().hit(Act::Start) && assetsReady()) play();
    // Y = defaults for the current tab
    if (!askKeep_ && !crash_ && !confirmRemove_ && !enhTab_.noticeOpen() && input().hit(Act::Y) && tab_ != Home && tab_ != Data && tab_ != Steam && tab_ != About) {
        if (tab_ == Display) cfg_.resetSection("display");
        else if (tab_ == Audio) cfg_.resetSection("audio");
        else if (tab_ == Controls) cfg_.resetSection("input");
        else if (tab_ == Game && enhTab_.page() != 0) cfg_.resetSection("enhancements");   // L8 pages
        else if (tab_ == Game) { cfg_.resetSection("game"); cfg_.resetSection("launcher"); }
        else if (tab_ == Language) { cfg_.game.language = "system"; lz::setLanguage("system"); }
        saveConfig();
        theme().sndConfirm();
    }
    footer(true);
}

void App::footer(bool tabs) {
    UI& u = ui();
    u.helpLine({10, 272, 460, 10});
    std::vector<Prompt> pr{{Act::Ok, T("prompt.select")}, {Act::Back, T("prompt.back")}};
    if (tabs) {
        pr.push_back({Act::TabPrev, ""});
        pr.push_back({Act::TabNext, T("prompt.tabs")});
        if (tab_ != Home && tab_ != Data && tab_ != Steam && tab_ != About) pr.push_back({Act::Y, T("prompt.defaults")});
        pr.push_back({Act::Start, T("prompt.play")});
    }
    u.prompts({8, 290, 464, 12}, pr);
    if (!toast_.empty() && now_ < toastUntil_)
        theme().text(toast_, 240, 306, Theme::kSmall, pal::emphasis, Align::Center, 0);
    else
        theme().text(T("app.unofficial"), 472, 306, Theme::kSmall, pal::textDim, Align::Right);
}

void App::tabHome(Rect c) {
    UI& u = ui();
    theme().logo(kW / 2, c.y + 2);
    int y = c.y + 92;
    bool ok = assetsReady();
    theme().text(ok ? Tf("home.files_ok", {cfg_.game.dumpVersion.empty() ? "?" : cfg_.game.dumpVersion})
                    : T("home.files_missing"),
                 c.x, y, Theme::kSmall, ok ? pal::ok : pal::select);
    std::string pad = input().gamepadName();
    theme().text(pad.empty() ? T("home.no_controller") : Tf("home.controller", {upper(pad)}), c.x, y + 11,
                 Theme::kSmall, pal::textDim);
    theme().text(zig::Paths::portable() ? T("home.portable") : Tf("home.data", {"%APPDATA%/ZigguratRecomp"}), c.x,
                 y + 22, Theme::kSmall, pal::textDim);
    // records
    int rx = c.x + c.w / 2 + 10;
    theme().text(T("home.stats"), rx, y, Theme::kSmall, pal::emphasis);
    if (games_ == 0) theme().text(T("home.no_stats"), rx, y + 11, Theme::kSmall, pal::textDim);
    else {
        auto row = [&](const char* id, const std::string& v, int dy) {
            theme().text(T(id), rx, y + dy, Theme::kSmall, pal::btnBorder);
            theme().text(v, c.x + c.w, y + dy, Theme::kSmall, pal::text, Align::Right);
        };
        row("home.games", std::to_string(games_), 11);
        row("home.best_kills", std::to_string(bestKills_), 22);
        row("home.best_seconds", fmt1(bestSeconds_) + " S", 33);
        row("home.total_kills", std::to_string(totalKills_), 44);
    }
    Rect pb{c.x + c.w / 2 - 90, c.y + c.h - 34, 180, 32};
    if (game_.running()) {
        theme().text(T("home.running"), c.x, pb.y + 10, Theme::kSmall, pal::emphasis, Align::Center, c.w);
    } else {
        if (u.focused().empty() || u.focused().rfind("tabs#", 0) == 0) {
            if (u.focused().empty()) u.focus("home.play");
        }
        if (u.bigButton("home.play", pb, T("btn.play"), T("home.play.help"))) {
            if (ok) play();
            else enterWizard(Wiz::Locate);
        }
    }
}

void App::refreshDisplayLists() {
    monitors_.clear();
    scales_.clear();
    fsModes_.clear();
    int n = 0;
    SDL_DisplayID* ids = SDL_GetDisplays(&n);
    SDL_DisplayID target = 0;
    for (int i = 0; i < n; i++) {
        const char* nm = SDL_GetDisplayName(ids[i]);
        monitors_.push_back(nm ? nm : ("DISPLAY " + std::to_string(i + 1)));
        if (cfg_.display.monitor == monitors_.back()) target = ids[i];
    }
    if (!target && n > 0) target = ids[0];
    SDL_Rect b{0, 0, 1920, 1080};
    if (target) SDL_GetDisplayUsableBounds(target, &b);
    for (int k = 1; 480 * k <= b.w && 320 * k <= b.h; k++) scales_.push_back(k);
    if (scales_.empty()) scales_.push_back(1);
    fsModes_.push_back("desktop");
    if (target) {
        int m = 0;
        SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(target, &m);
        std::set<std::string> seen;
        for (int i = 0; i < m; i++) {
            char s[48];
            snprintf(s, sizeof s, "%dx%d@%d", modes[i]->w, modes[i]->h, (int)std::lround(modes[i]->refresh_rate));
            if (seen.insert(s).second) fsModes_.push_back(s);
        }
        SDL_free(modes);
    }
    SDL_free(ids);
}

void App::tabDisplay(Rect c) {
    UI& u = ui();
    if (monitors_.empty()) refreshDisplayLists();
    auto& d = cfg_.display;
    bool ch = false;
    int y = c.y;
    auto row = [&]() { Rect r{c.x, y, c.w, kRowH}; y += kRowH + kRowGap; return r; };

    static const std::vector<std::string> modeKeys{"windowed", "borderless", "exclusive"};
    int mi = indexOf(modeKeys, d.mode);
    if (u.selector("d.mode", row(), T("disp.mode"), {T("disp.mode.windowed"), T("disp.mode.borderless"), T("disp.mode.exclusive")},
                   mi, T("disp.mode.help"))) { d.mode = modeKeys[mi]; ch = true; }
    std::vector<std::string> mons = monitors_;
    for (auto& m : mons) m = upper(m);
    int moni = indexOf(monitors_, d.monitor);
    if (u.selector("d.monitor", row(), T("disp.monitor"), mons, moni, T("disp.monitor.help"))) {
        d.monitor = monitors_[moni];
        refreshDisplayLists();
        ch = true;
    }
    std::vector<std::string> sizes;
    int si = 0;
    for (size_t i = 0; i < scales_.size(); i++) {
        sizes.push_back(std::to_string(480 * scales_[i]) + " X " + std::to_string(320 * scales_[i]) + " (X" +
                        std::to_string(scales_[i]) + ")");
        if (scales_[i] == d.windowScale) si = (int)i;
    }
    bool custom = d.customWidth > 0 && d.customHeight > 0;
    if (custom) {
        sizes.push_back("CUSTOM " + std::to_string(d.customWidth) + " X " + std::to_string(d.customHeight));
        si = (int)sizes.size() - 1;
    }
    if (u.selector("d.size", row(), T("disp.size"), sizes, si, T("disp.size.help"))) {
        if (si < (int)scales_.size()) { d.windowScale = scales_[si]; d.customWidth = d.customHeight = 0; }
        ch = true;
    }
    std::vector<std::string> fs;
    for (auto& m : fsModes_) fs.push_back(m == "desktop" ? T("disp.fsres.desktop") : upper(m) + " HZ");
    int fi = indexOf(fsModes_, d.fullscreenResolution);
    if (u.selector("d.fsres", row(), T("disp.fsres"), fs, fi, T("disp.fsres.help"))) { d.fullscreenResolution = fsModes_[fi]; ch = true; }
    if (u.toggle("d.int", row(), T("disp.integer"), d.integerScaling, T("disp.integer.help"))) ch = true;
    static const std::vector<std::string> filterKeys{"nearest", "sharp_bilinear", "linear"};
    int fli = indexOf(filterKeys, d.filter);
    if (u.selector("d.filter", row(), T("disp.filter"), {T("disp.filter.nearest"), T("disp.filter.sharp"), T("disp.filter.linear")},
                   fli, T("disp.filter.help"))) { d.filter = filterKeys[fli]; ch = true; }
    static const std::vector<std::string> aspectKeys{"original", "stretch"};
    int ai = indexOf(aspectKeys, d.aspect);
    if (u.selector("d.aspect", row(), T("disp.aspect"), {T("disp.aspect.original"), T("disp.aspect.stretch")}, ai,
                   T("disp.aspect.help"))) { d.aspect = aspectKeys[ai]; ch = true; }
    static const std::vector<std::string> borderKeys{"black", "night_sky", "sunset", "blur"};
    int bi = indexOf(borderKeys, d.borders);
    if (u.selector("d.borders", row(), T("disp.borders"),
                   {T("disp.borders.black"), T("disp.borders.night"), T("disp.borders.sunset"), T("disp.borders.blur")}, bi,
                   T("disp.borders.help"))) { d.borders = borderKeys[bi]; ch = true; }
    if (u.toggle("d.vsync", row(), T("disp.vsync"), d.vsync, T("disp.vsync.help"))) ch = true;
    int fpsi = d.fps == 60 ? 1 : 0;
    if (u.selector("d.fps", row(), T("disp.fps"), {T("disp.fps.30"), T("disp.fps.60")}, fpsi, T("disp.fps.help"))) {
        d.fps = fpsi ? 60 : 30;
        ch = true;
    }
    {
        auto r = row();
        r.w -= 140;  // leave room for APPLY & TEST at the bottom right
        if (u.toggle("d.showfps", r, T("disp.showfps"), d.showFps, T("disp.showfps.help"))) ch = true;
    }
    if (ch) saveConfig();
    if (u.focused().empty() || u.focused().rfind("tabs#", 0) == 0) { if (u.focused().empty()) u.focus("d.mode"); }
    if (!game_.running() && u.button("d.test", {c.x + c.w - 130, c.y + c.h - 20, 130, 20}, T("disp.test"), T("disp.test.help")))
        startPreview();
}

void App::refreshAudioDevices() {
    audioDevices_.clear();
    int n = 0;
    if (SDL_AudioDeviceID* ids = SDL_GetAudioPlaybackDevices(&n)) {
        for (int i = 0; i < n; i++)
            if (const char* nm = SDL_GetAudioDeviceName(ids[i])) audioDevices_.push_back(nm);
        SDL_free(ids);
    }
}

void App::tabAudio(Rect c) {
    UI& u = ui();
    auto& a = cfg_.audio;
    bool ch = false;
    int y = c.y;
    auto row = [&]() { Rect r{c.x, y, c.w, kRowH}; y += kRowH + kRowGap; return r; };
    if (u.slider("a.master", row(), T("aud.master"), a.master, 0, 1, 0.05f, T("aud.master.help"), pct(a.master))) ch = true;
    if (u.slider("a.music", row(), T("aud.music"), a.music, 0, 1, 0.05f, T("aud.music.help"), pct(a.music))) ch = true;
    if (u.slider("a.sfx", row(), T("aud.sfx"), a.sfx, 0, 1, 0.05f, T("aud.sfx.help"), pct(a.sfx))) ch = true;
    std::vector<std::string> devs{T("aud.device.default")};
    for (auto& d : audioDevices_) devs.push_back(upper(d));
    int di = 0;
    for (size_t i = 0; i < audioDevices_.size(); i++)
        if (audioDevices_[i] == a.device) di = (int)i + 1;
    if (u.selector("a.device", row(), T("aud.device"), devs, di, T("aud.device.help"))) {
        a.device = di == 0 ? "default" : audioDevices_[di - 1];
        ch = true;
    }
    if (u.toggle("a.mute", row(), T("aud.mute"), a.muteInBackground, T("aud.mute.help"))) ch = true;
    if (u.toggle("a.lmusic", row(), T("aud.launcher_music"), cfg_.launcher.music, T("aud.launcher_music.help"))) {
        audio::music(cfg_.launcher.music ? theme().musicPath() : "", 0.5f);
        ch = true;
    }
    if (ch) {
        audio::setVolumes(a.master, a.music, a.sfx);
        saveConfig();
    }
    if (u.focused().empty()) u.focus("a.master");
    y += 8;
    std::string dir = cfg_.assetsDir() + "/";
    if (u.button("a.tsound", {c.x, y, 130, 22}, T("aud.test_sound"), T("aud.test_sound.help")))
        audio::play(dir + "shoot.wav", 1.0f);
    if (u.button("a.tmusic", {c.x + 140, y, 130, 22}, T("aud.test_music"), T("aud.test_music.help"))) {
        audio::music(theme().musicPath(), 0.8f);
        musicStopAt_ = now_ + 5;
    }
}

namespace {
const std::vector<std::string> kPadBindNames{"a", "b", "x", "y", "lb", "rb", "lt", "rt", "lstick", "rstick",
                                             "back", "start", "dpad_up", "dpad_down", "dpad_left", "dpad_right", ""};
const std::vector<std::string> kPadActions{"press", "press_alt", "menu_tap", "recenter", "gyro_hold"};
const std::vector<std::string> kKeyActions{"press", "left", "right", "up", "down", "recenter"};
std::string bindLabel(const std::string& b) { return b.empty() ? T("ctl.none") : upper(b); }
}  // namespace

zig::PadState App::readLauncherPad() const {
    zig::PadState s;
    SDL_Gamepad* p = input().activePad();
    if (p) {
        s.connected = true;
        auto ax = [&](SDL_GamepadAxis a) { return SDL_GetGamepadAxis(p, a) / 32767.0f; };
        s.lx = ax(SDL_GAMEPAD_AXIS_LEFTX); s.ly = ax(SDL_GAMEPAD_AXIS_LEFTY);
        s.rx = ax(SDL_GAMEPAD_AXIS_RIGHTX); s.ry = ax(SDL_GAMEPAD_AXIS_RIGHTY);
        s.lt = ax(SDL_GAMEPAD_AXIS_LEFT_TRIGGER); s.rt = ax(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
        static const SDL_GamepadButton map[zig::PB_COUNT] = {
            SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH,
            SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_GUIDE, SDL_GAMEPAD_BUTTON_START, SDL_GAMEPAD_BUTTON_LEFT_STICK,
            SDL_GAMEPAD_BUTTON_RIGHT_STICK, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
            SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT,
            SDL_GAMEPAD_BUTTON_DPAD_RIGHT};
        for (int i = 0; i < zig::PB_COUNT; i++)
            if (SDL_GetGamepadButton(p, map[i])) s.buttons |= 1u << i;
        if (cfg_.input.gamepad.gyro.enabled && SDL_GamepadHasSensor(p, SDL_SENSOR_GYRO)) {
            if (!SDL_GamepadSensorEnabled(p, SDL_SENSOR_GYRO)) SDL_SetGamepadSensorEnabled(p, SDL_SENSOR_GYRO, true);
            float g[3] = {0, 0, 0};
            SDL_GetGamepadSensorData(p, SDL_SENSOR_GYRO, g, 3);
            s.gyroX = g[0]; s.gyroY = g[1]; s.gyroZ = g[2];
        }
    }
    const bool* ks = SDL_GetKeyboardState(nullptr);
    auto key = [&](const char* a) {
        auto it = cfg_.input.keyboard.keys.find(a);
        if (it == cfg_.input.keyboard.keys.end()) return false;
        SDL_Keycode k = SDL_GetKeyFromName(it->second.c_str());
        SDL_Scancode sc = SDL_GetScancodeFromKey(k, nullptr);
        return k != SDLK_UNKNOWN && sc != SDL_SCANCODE_UNKNOWN && ks[sc];
    };
    s.kbPress = key("press");
    s.kbRecenter = key("recenter");
    s.kbX = (key("right") ? 1.f : 0.f) - (key("left") ? 1.f : 0.f);
    s.kbY = (key("down") ? 1.f : 0.f) - (key("up") ? 1.f : 0.f);
    return s;
}

// Live preview of the virtual finger (same zig::InputMap code the game uses), 1/2 scale.
void App::controlsTest(Rect a) {
    Uint64 ns = SDL_GetTicksNS();
    float dt = testLastNs_ ? (float)((ns - testLastNs_) / 1e9) : 1.0f / 60;
    testLastNs_ = ns;
    if (dt > 0.1f) dt = 0.1f;
    int scheme = cfg_.game.controlScheme == "slingshot" ? 1 : 0;
    testMap_.update(readLauncherPad(), cfg_.input, scheme, zig::InputContext::Gameplay, dt);
    auto& g = gfx();
    g.fill(a, pal::night0);
    const float k = a.w / 480.0f;
    auto X = [&](float x) { return a.x + (int)(x * k); };
    auto Y = [&](float y) { return a.y + (int)(y * k); };
    for (int step = 0; step < 4; step++) {  // ziggurat silhouette
        int w = 120 + step * 80, y0 = 170 + step * 37;
        g.fill({X(240 - w / 2.f), Y((float)y0), (int)(w * k), (int)(37 * k) + 1}, pal::zigDark);
    }
    float hx = 240, hy = 160 - 12;  // hero / gun pivot (W/2, H-160)
    g.fill({X(hx) - 2, Y(hy) - 4, 4, 6}, pal::logoYellow);
    const auto& f = testMap_.finger();
    if (scheme == 0) {
        // precision: touch zone + aim ray at angle x/W*270+135
        g.fill({a.x, Y(zig::InputMap::kPrecisionY) - 1, a.w, 2}, pal::night3);
        float ang = zig::precisionAngleForX(testMap_.aimX(), 480) * 0.017453292f;
        g.line(X(hx), Y(hy), X(hx + std::cos(ang) * 200), Y(hy + std::sin(ang) * 200), f.down ? pal::select : pal::textDim);
        g.fill({X(testMap_.aimX()) - 2, Y(zig::InputMap::kPrecisionY) - 2, 5, 5}, f.down ? pal::select : pal::btnBorder);
        theme().text(std::to_string((int)zig::precisionAngleForX(testMap_.aimX(), 480)) + " DEG", a.x + 4, a.y + 4,
                     Theme::kSmall, pal::emphasis);
    } else {
        // slingshot: start circle (50 px) and pull vector; the shot flies opposite to the pull
        float cx = 240, cy = 160;
        for (int d = 0; d < 360; d += 12) {
            float r = d * 0.017453292f;
            g.fill({X(cx + std::cos(r) * 50), Y(cy + std::sin(r) * 50), 1, 1}, pal::iceShade);
        }
        if (f.down) {
            g.line(X(cx), Y(cy), X(f.x), Y(f.y), pal::select);
            g.line(X(hx), Y(hy), X(hx - (f.x - cx)), Y(hy - (f.y - cy)), pal::emphasis);
        }
        g.fill({X(f.down ? f.x : cx) - 2, Y(f.down ? f.y : cy) - 2, 5, 5}, f.down ? pal::select : pal::btnBorder);
        theme().text("SLINGSHOT", a.x + 4, a.y + 4, Theme::kSmall, pal::emphasis);
    }
}

void App::tabControls(Rect c) {
    UI& u = ui();
    auto& in = cfg_.input;
    auto& gp = in.gamepad;
    bool ch = false;
    int y = c.y;
    auto row = [&]() { Rect r{c.x, y, c.w, kRowH}; y += kRowH + kRowGap; return r; };

    // glyph override for the launcher's own prompts
    if (gp.glyphs == "xbox") input().forceDevice(Device::Xbox);
    else if (gp.glyphs == "playstation") input().forceDevice(Device::PlayStation);
    else if (gp.glyphs == "nintendo") input().forceDevice(Device::Nintendo);

    // ---- button / key capture (replaces the page while waiting, 5 s timeout)
    if (!captureAction_.empty()) {
        bool isKey = captureAction_.rfind("key:", 0) == 0;
        std::string action = captureAction_.substr(4);
        int left = (int)std::ceil(captureUntil_ - now_);
        u.textBlock({c.x, c.y + 90, c.w, 40},
                    Tf(isKey ? "ctl.capture.key" : "ctl.capture", {upper(action), std::to_string(std::max(left, 0))}),
                    Theme::kLarge, pal::select, Align::Center);
        std::string got;
        if (isKey) {
            for (SDL_Keycode k : input().keys) {
                if (k == SDLK_ESCAPE) { captureAction_.clear(); return; }
                if (k == SDLK_RETURN && now_ < captureUntil_ - 4.8) continue;  // the Enter that opened capture
                got = SDL_GetKeyName(k);
            }
            if (!got.empty()) in.keyboard.keys[action] = got;
        } else {
            zig::PadState s = readLauncherPad();
            uint32_t held = s.buttons | (s.lt > 0.5f ? 1u << 30 : 0) | (s.rt > 0.5f ? 1u << 31 : 0);
            uint32_t fresh = held & ~captureBaseline_;
            captureBaseline_ &= held;  // released buttons may be pressed again
            if (fresh & (1u << 31)) got = "rt";
            else if (fresh & (1u << 30)) got = "lt";
            else
                for (int b = 0; b < zig::PB_COUNT && got.empty(); b++)
                    if (fresh & (1u << b)) got = zig::buttonName(b);
            if (!got.empty()) gp.bindings[action] = got;
        }
        if (!got.empty() || now_ > captureUntil_) {
            if (!got.empty()) saveConfig();
            captureAction_.clear();
        }
        return;
    }

    takeStartPage(ctlPage_);
    u.selector("c.page", row(), T("ctl.page"),
               {T("ctl.page.sticks"), T("ctl.page.gyro"), T("ctl.page.buttons"), T("ctl.page.test")}, ctlPage_,
               T("ctl.page.help"));
    y += 3;
    auto x2 = [](float v) { char b[16]; snprintf(b, sizeof b, "%.2fX", v); return std::string(b); };
    if (ctlPage_ == 0) {
        std::vector<std::string> devs{T("ctl.device.auto")};
        std::vector<std::string> guids{"auto"};
        for (SDL_Gamepad* p : input().pads()) {
            char buf[64];
            SDL_GUIDToString(SDL_GetJoystickGUIDForID(SDL_GetGamepadID(p)), buf, sizeof buf);
            devs.push_back(upper(SDL_GetGamepadName(p) ? SDL_GetGamepadName(p) : "?"));
            guids.push_back(buf);
        }
        int di = 0;
        for (size_t i = 0; i < guids.size(); i++) if (guids[i] == gp.guid) di = (int)i;
        if (u.selector("c.dev", row(), T("ctl.device"), devs, di, T("ctl.device.help"))) { gp.guid = guids[di]; ch = true; }
        static const std::vector<std::string> glyphKeys{"auto", "xbox", "playstation", "nintendo"};
        int gi = indexOf(glyphKeys, gp.glyphs);
        if (u.selector("c.glyph", row(), T("ctl.glyphs"), {T("ctl.glyphs.auto"), "XBOX", "PLAYSTATION", "NINTENDO"}, gi,
                       T("ctl.glyphs.help"))) { gp.glyphs = glyphKeys[gi]; ch = true; }
        static const std::vector<std::string> aimKeys{"slide", "direct", "stick"};
        int ai = indexOf(aimKeys, gp.aimMode);
        if (u.selector("c.aim", row(), T("ctl.aim"), {T("ctl.aim.slide"), T("ctl.aim.direct"), T("ctl.aim.stick")}, ai,
                       T("ctl.aim.help"))) {
            gp.aimMode = aimKeys[ai]; ch = true;
        }
        if (u.slider("c.sens", row(), T("ctl.sens"), gp.sensitivity, 0.25f, 3.0f, 0.05f, T("ctl.sens.help"), x2(gp.sensitivity))) ch = true;
        if (u.slider("c.dzl", row(), T("ctl.dzl"), gp.deadzoneLeft, 0.0f, 0.5f, 0.01f, T("ctl.dzl.help"), pct(gp.deadzoneLeft))) ch = true;
        if (u.slider("c.dzr", row(), T("ctl.dzr"), gp.deadzoneRight, 0.0f, 0.5f, 0.01f, T("ctl.dzr.help"), pct(gp.deadzoneRight))) ch = true;
        if (u.slider("c.outer", row(), T("ctl.outer"), gp.outerDeadzone, 0.6f, 1.0f, 0.01f, T("ctl.outer.help"), pct(gp.outerDeadzone))) ch = true;
        static const std::vector<std::string> curveKeys{"linear", "quadratic"};
        int ci = indexOf(curveKeys, gp.curve);
        if (u.selector("c.curve", row(), T("ctl.curve"), {T("ctl.curve.linear"), T("ctl.curve.quadratic")}, ci,
                       T("ctl.curve.help"))) { gp.curve = curveKeys[ci]; ch = true; }
        if (u.slider("c.cursor", row(), T("ctl.cursor"), gp.cursorSpeed, 0.25f, 3.0f, 0.05f, T("ctl.cursor.help"), x2(gp.cursorSpeed))) ch = true;
        if (u.toggle("c.mouse", row(), T("ctl.mouse"), in.mouseAsTouch, T("ctl.mouse.help"))) ch = true;
    } else if (ctlPage_ == 1) {
        auto& gy = gp.gyro;
        if (u.toggle("c.gyro", row(), T("ctl.gyro"), gy.enabled, T("ctl.gyro.help"))) ch = true;
        int mi = gy.mode == "hold" ? 1 : 0;
        if (u.selector("c.gmode", row(), T("ctl.gyro.mode"), {T("ctl.gyro.always"), T("ctl.gyro.hold")}, mi,
                       T("ctl.gyro.mode.help"))) { gy.mode = mi ? "hold" : "always"; ch = true; }
        if (u.slider("c.gsens", row(), T("ctl.gyro.sens"), gy.sensitivity, 0.1f, 4.0f, 0.05f, T("ctl.gyro.sens.help"), x2(gy.sensitivity))) ch = true;
        if (u.toggle("c.gix", row(), T("ctl.gyro.ix"), gy.invertX, T("ctl.gyro.ix.help"))) ch = true;
        if (u.toggle("c.giy", row(), T("ctl.gyro.iy"), gy.invertY, T("ctl.gyro.iy.help"))) ch = true;
        y += 6;
        if (u.slider("c.rumble", row(), T("ctl.rumble"), gp.rumble, 0.0f, 1.0f, 0.05f, T("ctl.rumble.help"),
                     gp.rumble <= 0 ? std::string("OFF") : pct(gp.rumble))) ch = true;
        Rect br = row();
        if (u.button("c.rtest", {br.x, br.y + 4, 130, 18}, T("ctl.rumble.test"), T("ctl.rumble.test.help"))) {
            if (gp.rumble <= 0) { toast_ = T("ctl.rumble.off"); toastUntil_ = now_ + 2; }
            else if (SDL_Gamepad* p = input().activePad())
                SDL_RumbleGamepad(p, (Uint16)(0xFFFF * gp.rumble), (Uint16)(0xC000 * gp.rumble), 400);
        }
        std::string pad = input().gamepadName();
        theme().text(pad.empty() ? T("home.no_controller") : Tf("ctl.detected", {upper(pad)}), c.x + 140, br.y + 9,
                     Theme::kSmall, pad.empty() ? pal::textDim : pal::ok);
    } else if (ctlPage_ == 2) {
        std::map<std::string, int> uses;
        std::vector<std::string> labels;
        for (auto& n : kPadBindNames) labels.push_back(bindLabel(n));
        for (auto& a : kPadActions) {
            std::string b = gp.bindings[a];
            if (a != "menu_tap" && !b.empty()) uses[b]++;  // menu tap only acts in menus
            int bi = indexOf(kPadBindNames, b);
            std::string id = "c.pad." + a;
            if (u.selector(id, row(), T(("ctl.bind." + a).c_str()), labels, bi, T("ctl.bind.help"))) {
                gp.bindings[a] = kPadBindNames[bi]; ch = true;
            }
            if (u.isFocused(id) && (input().hit(Act::X) || input().key(SDLK_RETURN))) {
                captureAction_ = "pad:" + a;
                captureUntil_ = now_ + 5;
                zig::PadState s = readLauncherPad();
                captureBaseline_ = s.buttons | (s.lt > 0.5f ? 1u << 30 : 0) | (s.rt > 0.5f ? 1u << 31 : 0);
            }
        }
        y += 3;
        for (auto& a : kKeyActions) {
            std::string id = "c.key." + a;
            int zero = 0;
            u.selector(id, row(), T(("ctl.key." + a).c_str()), {upper(in.keyboard.keys[a])}, zero, T("ctl.bind.help"));
            if (u.isFocused(id) && (input().hit(Act::X) || input().key(SDLK_RETURN))) {
                captureAction_ = "key:" + a;
                captureUntil_ = now_ + 5;
            }
        }
        for (auto& [b, n] : uses)
            if (n > 1) theme().text(Tf("ctl.conflict", {upper(b)}), c.x, c.y + c.h - 10, Theme::kSmall, pal::select);
    } else {
        u.textBlock({c.x, y, c.w, 20}, T("ctl.test.text"), Theme::kSmall, pal::btnBorder);
        y += 20;
        Rect area{c.x + (c.w - 240) / 2, y, 240, 160};
        // focusable frame so the stick/arrows don't navigate away while testing
        u.button("c.testarea", {area.x - 2, area.y - 2, area.w + 4, area.h + 4}, "", T("ctl.test.area.help"));
        controlsTest(area);
    }
    if (ch) saveConfig();
    if (u.focused().empty()) u.focus("c.page");
}

void App::tabLanguage(Rect c) {
    UI& u = ui();
    std::vector<std::string> items{T("lang.system")};
    for (auto& l : lz::languages()) items.push_back(l.name);
    int sel = 0;
    for (size_t i = 0; i < lz::languages().size(); i++)
        if (cfg_.game.language == lz::languages()[i].code) sel = (int)i + 1;
    theme().text(T("lang.title"), c.x, c.y, Theme::kLarge);
    int before = sel;
    bool act = u.list("l.list", {c.x, c.y + 22, 220, c.h - 24}, items, sel, T("lang.help"), 16);
    if (act || sel != before) {
        cfg_.game.language = sel == 0 ? "system" : lz::languages()[sel - 1].code;
        lz::setLanguage(cfg_.game.language);
        saveConfig();
    }
    u.textBlock({c.x + 236, c.y + 24, c.w - 236, 80}, T("lang.help"), Theme::kSmall, pal::btnBorder);
    if (u.focused().empty()) u.focus("l.list");
}

void App::tabGame(Rect c) {
    UI& u = ui();
    // L8: pages ACCESSIBILITY / ORIGINAL BUGS / PRACTICE / DEVELOPER (screens/EnhancementsTab.cpp)
    if (enhTab_.pageSelector(c, cfg_.enhancements) != 0) {
        if (enhTab_.frame(c, cfg_.enhancements)) saveConfig();
        return;
    }
    auto& g = cfg_.game;
    auto& l = cfg_.launcher;
    bool ch = false;
    int y = c.y;
    auto row = [&]() { Rect r{c.x, y, c.w, kRowH}; y += kRowH + kRowGap; return r; };
    int si = g.controlScheme == "slingshot" ? 1 : 0;
    if (u.selector("g.scheme", row(), T("game.scheme"), {T("game.scheme.precision"), T("game.scheme.slingshot")}, si,
                   T("game.scheme.help"))) { g.controlScheme = si ? "slingshot" : "precision"; ch = true; }
    if (u.toggle("g.aim", row(), T("game.aim"), g.aimGuide, T("game.aim.help"))) ch = true;
    if (u.toggle("g.tut", row(), T("game.tutorial"), g.showTutorial, T("game.tutorial.help"))) ch = true;
    y += 8;
    static const std::vector<std::string> onPlayKeys{"close", "minimize", "stay"};
    int oi = indexOf(onPlayKeys, l.onPlay, 1);
    if (u.selector("g.onplay", row(), T("launch.onplay"),
                   {T("launch.onplay.close"), T("launch.onplay.minimize"), T("launch.onplay.stay")}, oi,
                   T("launch.onplay.help"))) { l.onPlay = onPlayKeys[oi]; ch = true; }
    static const std::vector<std::string> steamKeys{"show_launcher", "start_game"};
    int sti = indexOf(steamKeys, l.fromSteam);
    if (u.selector("g.steam", row(), T("launch.steam"), {T("launch.steam.show"), T("launch.steam.start")}, sti,
                   T("launch.steam.help"))) { l.fromSteam = steamKeys[sti]; ch = true; }
    if (ch) saveConfig();
    if (u.focused().empty()) u.focus("g.scheme");
}

std::string App::supportInfo() const {
    std::string s;
    s += "ZiGGURAT launcher (unofficial port) - support info\n";
    s += std::string("Version: ") + zig::kVersion + "\n";
    s += std::string("Platform: ") + SDL_GetPlatform() + "\n";
    s += std::string("SDL: ") + std::to_string(SDL_VERSIONNUM_MAJOR(SDL_GetVersion())) + "." +
         std::to_string(SDL_VERSIONNUM_MINOR(SDL_GetVersion())) + "." + std::to_string(SDL_VERSIONNUM_MICRO(SDL_GetVersion())) + "\n";
    if (const char* r = SDL_GetRendererName(const_cast<Gfx&>(gfx()).renderer())) s += std::string("Renderer: ") + r + "\n";
    int n = 0;
    if (SDL_DisplayID* ids = SDL_GetDisplays(&n)) {
        for (int i = 0; i < n; i++) {
            const SDL_DisplayMode* m = SDL_GetDesktopDisplayMode(ids[i]);
            const char* nm = SDL_GetDisplayName(ids[i]);
            s += "Display " + std::to_string(i + 1) + ": " + (nm ? nm : "?") +
                 (m ? " " + std::to_string(m->w) + "x" + std::to_string(m->h) + "@" + std::to_string((int)std::lround(m->refresh_rate)) : "") + "\n";
        }
        SDL_free(ids);
    }
    std::string pad = const_cast<Input&>(input()).gamepadName();
    s += "Controller: " + (pad.empty() ? std::string("none") : pad) + "\n";
    s += "Game dump: version " + (cfg_.game.dumpVersion.empty() ? std::string("-") : cfg_.game.dumpVersion) +
         (cfg_.game.dumpVerified ? " (verified)" : " (unverified)") + ", assets " + (assetsReady() ? "ok" : "missing") + "\n";
    s += std::string("Portable: ") + (zig::Paths::portable() ? "yes" : "no") + "\n";
    const auto& d = cfg_.display;
    s += "Display: mode=" + d.mode + " scale=" + std::to_string(d.windowScale) + " fsres=" + d.fullscreenResolution +
         " integer=" + (d.integerScaling ? "1" : "0") + " filter=" + d.filter + " aspect=" + d.aspect + " borders=" + d.borders +
         " vsync=" + (d.vsync ? "1" : "0") + " fps=" + std::to_string(d.fps) + "\n";
    const auto& a = cfg_.audio;
    s += "Audio: master=" + pct(a.master) + " music=" + pct(a.music) + " sfx=" + pct(a.sfx) + " device=" + a.device + "\n";
    s += "Language: " + cfg_.game.language + " (resolved " + lz::language() + "), scheme=" + cfg_.game.controlScheme + "\n";
    s += DataTab::supportLines();   // L7: records summary (counts only) + enhancement preset
    std::string log = newestGameLog();
    if (!log.empty()) {
        s += "Last game log (tail):\n";
        for (auto& l : zig::Log::tail(log, 15)) s += "  " + l + "\n";
    }
    // anonymize: home folder -> ~, also the user name in backslash form
    if (const char* h = SDL_GetUserFolder(SDL_FOLDER_HOME)) {
        std::string home = h;
        while (!home.empty() && (home.back() == '/' || home.back() == '\\')) home.pop_back();
        std::vector<std::string> forms{home};
        std::string alt = home;
        for (auto& ch : alt) ch = ch == '\\' ? '/' : (ch == '/' ? '\\' : ch);
        forms.push_back(alt);
        for (auto& f : forms)
            for (size_t p; !f.empty() && (p = s.find(f)) != std::string::npos;) s.replace(p, f.size(), "~");
        std::string user = fs::u8path(home).filename().u8string();
        if (user.size() > 2)
            for (size_t p; (p = s.find(user)) != std::string::npos;) s.replace(p, user.size(), "<user>");
    }
    return s;
}

void App::tabData(Rect c) {
    UI& u = ui();
    int dataPage = dataTab_.pageSelector(c);
    if (dataPage == 3) {   // P2: MODS & EXTRAS (screens/Extras.cpp)
        if (extras_.dataPage(c, cfg_)) saveConfig();
        return;
    }
    if (dataPage != 0) {   // L7: YOUR RECORDS / SAVES pages (screens/DataTab.cpp)
        dataTab_.frame(c, cfg_.assetsDir());
        if (dataTab_.consumeChanged()) loadStats();
        return;
    }
    theme().text(T("data.dump"), c.x, c.y, Theme::kSmall, pal::emphasis);
    std::string st = !assetsReady() ? T("data.dump.none")
                     : cfg_.game.dumpVerified ? Tf("data.dump.ok", {cfg_.game.dumpVersion})
                                              : Tf("data.dump.unverified", {cfg_.game.dumpVersion});
    theme().text(st, c.x, c.y + 11, Theme::kSmall, assetsReady() ? pal::ok : pal::select);
    int y = c.y + 28, bw = 200;
    if (u.button("data.reimport", {c.x, y, bw, 20}, T("data.reimport"), T("data.reimport.help"))) enterWizard(Wiz::Locate);
    if (u.button("data.remove", {c.x + bw + 10, y, bw, 20}, T("data.remove"), T("data.remove.help")) && assetsReady())
        confirmRemove_ = true;
    y += 26;
    if (u.button("data.open", {c.x, y, bw, 20}, T("data.open_data"), T("data.open_data.help"))) openFolder(zig::Paths::dataDir());
    if (u.button("data.logs", {c.x + bw + 10, y, bw, 20}, T("data.open_logs"), T("data.open_logs.help"))) openFolder(zig::Paths::logsDir());
    y += 26;
    if (u.button("data.assets", {c.x, y, bw, 20}, T("data.open_assets"), T("data.open_assets.help"))) openFolder(cfg_.assetsDir());
    if (u.button("data.support", {c.x + bw + 10, y, bw, 20}, T("data.support"), T("data.support.help"))) {
        SDL_SetClipboardText(supportInfo().c_str());
        toast_ = T("data.copied");
        toastUntil_ = now_ + 3;
        theme().sndConfirm();
    }
    y += 32;
    u.textBlock({c.x, y, c.w, 40}, zig::Paths::portable() ? T("data.portable.on") : T("data.portable.off"), Theme::kSmall,
                pal::textDim);
    if (u.focused().empty()) u.focus("data.reimport");
}

void App::tabAbout(Rect c) {
    UI& u = ui();
    if (extras_.aboutPageSelector(c) == 1) {   // P2: UPDATES page
        if (extras_.updatesPage(c, cfg_)) saveConfig();
        return;
    }
    int y = c.y;
    theme().text(T("about.game"), c.x, y, Theme::kSmall, pal::emphasis);
    y += 12;
    auto rest = [&] { return c.y + c.h - y; };
    y += u.textBlock({c.x, y, c.w, rest()}, T("about.staff"), Theme::kSmall, pal::text) + 3;
    y += u.textBlock({c.x, y, c.w, rest()}, T("about.thanks"), Theme::kSmall, pal::btnBorder) + 5;
    y += u.textBlock({c.x, y, c.w, rest()}, T("about.port"), Theme::kSmall, pal::btnBorder) + 3;
    y += u.textBlock({c.x, y, c.w, rest()}, T("about.licences"), Theme::kSmall, pal::btnBorder) + 5;
    u.textBlock({c.x, y, c.w, rest()}, T("about.disclaimer"), Theme::kSmall, pal::textDim);
    if (u.focused().empty()) u.focus("tabs#8");
}

// ------------------------------------------------------------------ Steam (L6)

namespace {
zui::Tex thumbTex(const std::string& path, int w, int h) {
    zui::Image src;
    if (!src.load(path) || src.w == 0) return {};
    zui::Image d;
    d.w = w;
    d.h = h;
    d.px.assign((size_t)w * h * 4, 0);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int x0 = x * src.w / w, x1 = std::max(x0 + 1, (x + 1) * src.w / w);
            int y0 = y * src.h / h, y1 = std::max(y0 + 1, (y + 1) * src.h / h);
            unsigned acc[4] = {0, 0, 0, 0}, n = 0;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++, n++)
                    for (int c = 0; c < 4; c++) acc[c] += src.at(xx, yy)[c];
            for (int c = 0; c < 4; c++) d.at(x, y)[c] = (uint8_t)(acc[c] / std::max(1u, n));
        }
    return gfx().texture(d);
}
}  // namespace

zig::ShortcutSpec App::steamSpec() const {
    zig::ShortcutSpec s;
#ifdef _WIN32
    s.exePath = zig::Paths::exeDir() + "ZigguratLauncher.exe";
#else
    s.exePath = zig::Paths::exeDir() + "ZigguratLauncher";
#endif
    s.startDir = zig::Paths::exeDir();
    while (!s.startDir.empty() && (s.startDir.back() == '/' || s.startDir.back() == '\\')) s.startDir.pop_back();
    return s;
}

std::string App::steamArtDir() const { return zig::Paths::dataDir() + "steam-art"; }

void App::steamRefresh(bool regenArt) {
    steam_ = zig::Steam::status();
    if (steamAcc_ >= (int)steam_.accounts.size()) steamAcc_ = 0;
    steamEntry_ = {};
    if (steam_.found && !steam_.accounts.empty())
        steamEntry_ = zig::Steam::entryStatus(steam_.root, steam_.accounts[steamAcc_].accountId, steamSpec());
    deskLink_ = zig::Shortcuts::exists(zig::ShortcutPlace::Desktop);
    startLink_ = zig::Shortcuts::exists(zig::ShortcutPlace::StartMenu);
    if (regenArt && !steamArtReady_ && assetsReady()) {
        auto r = zig::ArtworkGen::generate(cfg_.assetsDir(), steamArtDir());
        steamArtReady_ = r.ok;
        for (auto& t : steamThumbs_) gfx().destroy(t);
        steamThumbs_.clear();
        if (r.ok) {
            std::string d = steamArtDir() + "/";
            steamThumbs_.push_back(thumbTex(d + "portrait.png", 32, 48));
            steamThumbs_.push_back(thumbTex(d + "wide.png", 102, 48));
            steamThumbs_.push_back(thumbTex(d + "hero.png", 149, 48));
            steamThumbs_.push_back(thumbTex(d + "icon.png", 40, 40));
        }
    }
    steamNextPoll_ = now_ + 2;
}

void App::steamDo(int action) {
    if (!steam_.found || steam_.accounts.empty()) { ui().error(); return; }
    if (zig::Steam::isRunning()) {
        steamPending_ = action;
        steamAskClose_ = true;
        return;
    }
    const std::string acc = steam_.accounts[steamAcc_].accountId;
    zig::SteamResult r;
    if (action == 1) {
        auto art = zig::ArtworkGen::generate(cfg_.assetsDir(), steamArtDir());
        if (!art.ok) { toast_ = T("steam.err.art"); toastUntil_ = now_ + 4; ui().error(); return; }
        Extras::prepareLogoJson(steamArtDir(), cfg_.extras);   // P2: optional logo position (experimental)
        steamArtReady_ = false;  // reload thumbnails
        r = zig::Steam::addOrUpdate(steam_.root, acc, steamSpec(), steamArtDir());
    } else if (action == 2) {
        r = zig::Steam::remove(steam_.root, acc, steamSpec());
    } else if (action == 3) {
        r = zig::Steam::undo(steam_.root, acc);
    }
    toast_ = T(r.msgId.c_str());
    toastUntil_ = now_ + 5;
    if (r.ok) theme().sndConfirm(); else ui().error();
    steamRefresh(true);
}

void App::steamAdd() { steamDo(1); }

void App::steamPoll() {
    if (steamWaitClose_ && now_ >= steamNextPoll_) {
        steamNextPoll_ = now_ + 0.5;
        if (!zig::Steam::isRunning()) {
            steamWaitClose_ = false;
            toastUntil_ = 0;
            int a = steamPending_;
            steamPending_ = 0;
            steam_.running = false;
            if (a) steamDo(a);
        } else if (now_ > steamWaitUntil_) {
            steamWaitClose_ = false;
            steamPending_ = 0;
            toast_ = T("steam.wait_timeout");
            toastUntil_ = now_ + 5;
        }
    } else if (!steamWaitClose_ && (tab_ == Steam || wiz_ == Wiz::SteamAsk) && now_ >= steamNextPoll_) {
        steam_.running = zig::Steam::isRunning();
        steamNextPoll_ = now_ + 2;
    }
}

void App::tabSteam(Rect c) {
    UI& u = ui();
    if (steam_.found && extras_.steamPageSelector(c) == 1) {   // P2: EXPERIMENTAL page
        int r = extras_.steamPage(c, cfg_, steam_, steamAcc_, steamSpec(), steamEntry_.added);
        if (r >= 1) saveConfig();
        if (r == 2) steamDo(1);
        if (!extras_.toast.empty()) { toast_ = extras_.toast; toastUntil_ = now_ + 4; extras_.toast.clear(); }
        return;
    }
    int y = c.y;
    if (!steam_.found) {
        theme().text(T("steam.notfound"), c.x, y, Theme::kSmall, pal::select);
        u.textBlock({c.x, y + 14, c.w, 40}, T("steam.notfound.help"), Theme::kSmall, pal::textDim);
        y += 56;
    } else {
        std::string st = T("steam.found") + (steam_.sandbox ? T("steam.sandbox") : std::string());
        theme().text(st, c.x, y, Theme::kSmall, pal::ok);
        theme().text(steam_.running ? T("steam.running") : T("steam.closed"), c.x + c.w, y, Theme::kSmall,
                     steam_.running ? pal::select : pal::textDim, Align::Right);
        y += 12;
        if (steam_.accounts.empty()) {
            theme().text(T("steam.noacc"), c.x, y, Theme::kSmall, pal::select);
            y += 14;
        } else {
            std::vector<std::string> names;
            for (size_t i = 0; i < steam_.accounts.size(); i++) {
                // The game's pixel font only has ASCII: fall back to "ACCOUNT n" for other names.
                const std::string& pn = steam_.accounts[i].persona;
                bool ascii = !pn.empty() && std::all_of(pn.begin(), pn.end(), [](char ch) { return ch >= 32 && ch < 127; });
                names.push_back(ascii ? upper(pn) : Tf("steam.account.n", {std::to_string(i + 1)}));
            }
            int ai = steamAcc_;
            if (u.selector("st.acc", {c.x, y, c.w, kRowH}, T("steam.account"), names, ai, T("steam.account.help"))) {
                steamAcc_ = ai;
                steamRefresh(false);
            }
            y += kRowH + 3;
            theme().text(steamEntry_.added ? T("steam.in_library") : T("steam.not_added"), c.x, y, Theme::kSmall,
                         steamEntry_.added ? pal::ok : pal::emphasis);
            y += 13;
        }
        int bw = 140, bh = 18, gap = 6;
        bool canEdit = !steam_.accounts.empty() && assetsReady();
        if (u.button("st.add", {c.x, y, bw, bh}, steamEntry_.added ? T("steam.update") : T("steam.add"), T("steam.add.help"))) {
            if (canEdit) steamDo(1); else u.error();
        }
        if (u.button("st.remove", {c.x + bw + gap, y, bw, bh}, T("steam.remove"), T("steam.remove.help"))) {
            if (steamEntry_.added) steamDo(2); else u.error();
        }
        if (u.button("st.undo", {c.x + 2 * (bw + gap), y, c.w - 2 * (bw + gap), bh}, T("steam.undo"), T("steam.undo.help"))) {
            if (steamEntry_.canUndo) steamDo(3); else u.error();
        }
        y += bh + 4;
        if (u.button("st.close", {c.x, y, bw, bh}, T("steam.close"), T("steam.close.help"))) {
            if (steam_.running) { steamPending_ = 0; steamAskClose_ = true; } else u.error();
        }
        if (u.button("st.start", {c.x + bw + gap, y, bw, bh}, T("steam.start"), T("steam.start.help"))) {
            if (zig::Steam::start(steam_.root)) theme().sndConfirm();
            steamNextPoll_ = now_ + 3;
        }
        if (u.button("st.copy", {c.x + 2 * (bw + gap), y, c.w - 2 * (bw + gap), bh}, T("steam.copycmd"), T("steam.copycmd.help"))) {
            SDL_SetClipboardText(zig::Steam::launchCommand(steamSpec()).c_str());
            toast_ = T("steam.copied");
            toastUntil_ = now_ + 3;
            theme().sndConfirm();
        }
        y += bh + 6;
    }
    // desktop / start menu shortcuts (independent of Steam)
    int hw = (c.w - 8) / 2;
    bool d = deskLink_, s = startLink_;
    auto spec = steamSpec();
    if (u.toggle("st.desk", {c.x, y, hw, kRowH}, T("steam.desktop"), d, T("steam.desktop.help"))) {
        if (d) zig::Shortcuts::create(zig::ShortcutPlace::Desktop, spec.exePath, "", spec.startDir, "");
        else zig::Shortcuts::remove(zig::ShortcutPlace::Desktop);
        deskLink_ = zig::Shortcuts::exists(zig::ShortcutPlace::Desktop);
    }
    if (u.toggle("st.menu", {c.x + hw + 8, y, hw, kRowH}, T("steam.startmenu"), s, T("steam.startmenu.help"))) {
        if (s) zig::Shortcuts::create(zig::ShortcutPlace::StartMenu, spec.exePath, "", spec.startDir, "");
        else zig::Shortcuts::remove(zig::ShortcutPlace::StartMenu);
        startLink_ = zig::Shortcuts::exists(zig::ShortcutPlace::StartMenu);
    }
    y += kRowH + 4;
    // L9: the info text replaces the artwork caption (there is no room below the thumbnails)
    y += u.textBlock({c.x, y, c.w, 22}, T("steam.info"), Theme::kSmall, pal::textDim) + 1;
    if (steamThumbs_.size() == 4) {
        int xs[4] = {0, 38, 146, 306};
        gfx().setClip(&c);  // never paint over the ice frame
        for (int i = 0; i < 4; i++)
            if (steamThumbs_[i]) gfx().draw(steamThumbs_[i], c.x + xs[i], y + (i == 3 ? 4 : 0));
        gfx().setClip(nullptr);
    }
    if (u.focused().empty()) u.focus(steam_.found && !steam_.accounts.empty() ? "st.add" : "st.desk");
}

// ------------------------------------------------------------------ dialogs

void App::overlays() {
    UI& u = ui();
    steamPoll();
    if (steamAskClose_) {
        int r = u.messageBox("steamclose", T("steam.close.title"), T("steam.close.text"), {T("steam.close"), T("btn.cancel")});
        if (r == 0) {
            // User-initiated only: ask Steam to quit, then wait for it before applying the pending change.
            zig::Steam::requestShutdown(steam_.root);
            steamWaitClose_ = true;
            steamWaitUntil_ = now_ + 45;
            toast_ = T("steam.waiting");
            toastUntil_ = now_ + 45;
            steamAskClose_ = false;
        } else if (r == 1) {
            steamAskClose_ = false;
            steamPending_ = 0;
        }
    } else if (askKeep_) {
        int r = u.messageBox("keep", T("disp.keep.title"), T("disp.keep.text"), {T("btn.keep"), T("btn.revert")});
        if (r == 0) askKeep_ = false;
        if (r == 1) {
            cfg_.display = displayBeforeTest_;
            saveConfig();
            askKeep_ = false;
        }
    } else if (crash_) {
        std::string text = Tf("crash.text", {std::to_string(crashCode_)}) + "\n";
        int from = std::max(0, (int)crashLines_.size() - 8);
        for (int i = from; i < (int)crashLines_.size(); i++) text += crashLines_[i].substr(0, 80) + "\n";
        int r = u.messageBox("crash", T("crash.title"), text, {T("data.support"), T("btn.close")});
        if (r == 0) {
            SDL_SetClipboardText(supportInfo().c_str());
            toast_ = T("data.copied");
            toastUntil_ = now_ + 3;
            crash_ = false;
        } else if (r == 1) crash_ = false;
    } else if (confirmRemove_) {
        int r = u.messageBox("remove", T("data.remove"), T("data.remove.confirm"), {T("btn.yes"), T("btn.no")});
        if (r == 0) {
            // Only ever delete the launcher-managed copy inside the data folder.
            std::string dir = cfg_.assetsDir();
            fs::path pd = fs::weakly_canonical(fs::u8path(dir)), dd = fs::weakly_canonical(fs::u8path(zig::Paths::dataDir()));
            std::string ps = pd.u8string(), ds = dd.u8string();
            if (ps.rfind(ds, 0) == 0 && ps.size() > ds.size()) {
                std::error_code ec;
                fs::remove_all(pd, ec);
            }
            cfg_.game.dumpVerified = false;
            cfg_.game.dumpVersion.clear();
            saveConfig();
            reloadTheme(true);
            confirmRemove_ = false;
            enterWizard(Wiz::Welcome);
        } else if (r == 1) confirmRemove_ = false;
    }
}

}  // namespace zl
