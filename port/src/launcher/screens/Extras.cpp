#include "launcher/TestHooks.h"
#include "launcher/screens/Extras.h"

#include <SDL3/SDL.h>

#include <filesystem>
#include <fstream>
#include <vector>

#include "core/Paths.h"
#include "core/Reference.h"
#include "core/SteamInput.h"
#include "core/Version.h"
#include "launcher/Strings.h"
#include "launcher/ui/Theme.h"

namespace fs = std::filesystem;
using namespace zui;
using lz::T;
using lz::Tf;

namespace zl {
namespace {
constexpr int kRowH = 14;
const std::vector<std::string> kLogoKeys{"default", "BottomLeft", "UpperLeft", "CenterCenter", "UpperCenter",
                                         "BottomCenter"};
const std::vector<std::string> kSiKeys{"default", "enabled", "disabled"};
int indexOf(const std::vector<std::string>& v, const std::string& s) {
    for (size_t i = 0; i < v.size(); i++)
        if (v[i] == s) return (int)i;
    return 0;
}
void openDir(const std::string& path) {
    zig::makeDirs(path);
    std::string p = path;
    for (auto& ch : p) if (ch == '\\') ch = '/';
    std::string url = "file:///" + (p.size() && p[0] == '/' ? p.substr(1) : p);
    SDL_OpenURL(url.c_str());
}
}  // namespace

Extras::~Extras() {
    if (th_.joinable()) th_.join();
}

std::string Extras::overridesDir() { return zig::Paths::dataDir() + "overrides"; }

int Extras::overrideCount() {
    int n = 0;
    std::error_code ec;
    if (!fs::is_directory(fs::u8path(overridesDir()), ec)) return 0;
    for (auto& e : fs::recursive_directory_iterator(fs::u8path(overridesDir()), ec))
        if (e.is_regular_file(ec)) n++;
    return n;
}

// ------------------------------------------------------------------ DATA > MODS & EXTRAS
bool Extras::dataPage(Rect c, zig::Config& cfg) {
    UI& u = ui();
    bool ch = false;
    int y = c.y;
    theme().text(T("extras.ov.title"), c.x, y, Theme::kSmall, pal::emphasis);
    y += 12;
    bool on = cfg.extras.overrides;
    if (u.toggle("ex.ov", {c.x, y, c.w, kRowH}, T("extras.ov.use"), on, T("extras.ov.use.help"))) {
        cfg.extras.overrides = on;
        ch = true;
    }
    y += kRowH + 3;
    if (ovCount_ < 0 || u.now() >= ovNext_) {
        ovCount_ = overrideCount();
        ovNext_ = u.now() + 2;
    }
    theme().text(Tf("extras.ov.count", {std::to_string(ovCount_)}), c.x, y, Theme::kSmall,
                 ovCount_ > 0 ? pal::ok : pal::textDim);
    y += 12;
    if (u.button("ex.ovopen", {c.x, y, 200, 20}, T("extras.ov.open"), T("extras.ov.open.help"))) openDir(overridesDir());
    y += 26;
    y += u.textBlock({c.x, y, c.w, 40}, T("extras.ov.info"), Theme::kSmall, pal::textDim) + 8;

    if (cfg.enhancements.devOptions) {   // developer reference mode
        theme().text(T("extras.ref.title"), c.x, y, Theme::kSmall, pal::emphasis);
        y += 12;
        std::string exe = zig::Reference::findTouchHLE(cfg.extras.touchhlePath);
        theme().text(exe.empty() ? T("extras.ref.notfound") : Tf("extras.ref.found", {fs::u8path(exe).filename().u8string()}),
                     c.x, y, Theme::kSmall, exe.empty() ? pal::select : pal::ok);
        y += 12;
        if (u.button("ex.ref", {c.x, y, 200, 20}, T("extras.ref.run"), T("extras.ref.run.help"))) {
            std::string err;
            if (exe.empty()) {
                refStatus_ = T("extras.ref.notfound");
                u.error();
            } else {
                std::string app = zig::Reference::appFolder(cfg.game.dumpSource, err);
                if (app.empty() || !zig::Reference::launch(exe, app, err)) {
                    refStatus_ = Tf("extras.ref.err", {err});
                    u.error();
                } else {
                    refStatus_ = T("extras.ref.started");
                    theme().sndConfirm();
                }
            }
        }
        y += 24;
        if (!refStatus_.empty()) u.textBlock({c.x, y, c.w, 20}, refStatus_, Theme::kSmall, pal::textDim);
    }
    if (u.focused().empty()) u.focus("ex.ov");
    return ch;
}

// ------------------------------------------------------------------ STEAM > EXPERIMENTAL
int Extras::steamPageSelector(Rect& c) {
    UI& u = ui();
    takeStartPage(steamPage_);
    u.selector("st.page", {c.x, c.y, c.w, kRowH}, T("extras.st.page"), {T("extras.st.page.lib"), T("extras.st.page.exp")},
               steamPage_, T("extras.st.page.help"));
    c.y += kRowH + 4;
    c.h -= kRowH + 4;
    return steamPage_;
}

void Extras::prepareLogoJson(const std::string& artDir, const zig::ExtrasCfg& x) {
    std::string path = artDir + "/logo.json";
    std::error_code ec;
    if (x.logoPosition.empty() || x.logoPosition == "default") {
        fs::remove(fs::u8path(path), ec);   // Steam's own placement: write nothing (never delete Steam's file)
        return;
    }
    int pct = std::max(10, std::min(100, x.logoSizePct));
    std::ofstream o(fs::u8path(path), std::ios::trunc);
    o << "{\"nVersion\":1,\"logoPosition\":{\"pinnedPosition\":\"" << x.logoPosition << "\",\"nWidthPct\":" << pct
      << ",\"nHeightPct\":" << pct << "}}";
}

int Extras::steamPage(Rect c, zig::Config& cfg, const zig::SteamStatus& st, int account, const zig::ShortcutSpec& spec,
                      bool added) {
    UI& u = ui();
    int result = 0;
    int y = c.y;
    theme().text(T("extras.st.warn"), c.x, y, Theme::kSmall, pal::select);
    y += 12;
    // --- logo position
    int li = indexOf(kLogoKeys, cfg.extras.logoPosition);
    if (u.selector("ex.logo", {c.x, y, c.w, kRowH}, T("extras.logo"),
                   {T("extras.logo.default"), T("extras.logo.bl"), T("extras.logo.ul"), T("extras.logo.cc"),
                    T("extras.logo.uc"), T("extras.logo.bc")},
                   li, T("extras.logo.help"))) {
        cfg.extras.logoPosition = kLogoKeys[li];
        result = 1;
    }
    y += kRowH + 1;
    float pct = (float)cfg.extras.logoSizePct;
    if (u.slider("ex.logosz", {c.x, y, c.w, kRowH}, T("extras.logo.size"), pct, 10, 100, 5, T("extras.logo.size.help"),
                 std::to_string((int)pct) + "%")) {
        cfg.extras.logoSizePct = (int)pct;
        result = 1;
    }
    y += kRowH + 3;
    bool haveAcc = st.found && account >= 0 && account < (int)st.accounts.size();
    if (haveAcc && u.now() >= logoJsonNext_) {
        logoJson_ = zig::Steam::readLogoJson(st.root, st.accounts[account].accountId, spec);
        logoJsonNext_ = u.now() + 2;
    }
    std::string shown;   // add break opportunities so the one-line JSON wraps inside the panel
    for (char ch : logoJson_.substr(0, 110)) {
        if (ch == '\r' || ch == '\n') continue;
        shown += ch;
        if (ch == ',' || ch == '{') shown += ' ';
    }
    std::string det = logoJson_.empty() ? T("extras.logo.none") : Tf("extras.logo.detected", {shown});
    y += u.textBlock({c.x, y, c.w, 22}, det, Theme::kSmall, pal::textDim) + 3;
    if (u.button("ex.logoapply", {c.x, y, 200, 18}, added ? T("extras.logo.apply") : T("steam.add"),
                 T("extras.logo.apply.help"))) {
        if (haveAcc) result = 2; else u.error();
    }
    y += 24;
    // --- Steam Input mode
    if (haveAcc) {
        zig::ShortcutIds ids = zig::Steam::idsFor(zig::Steam::quote(spec.exePath), spec.appName);
        std::string key = st.root + "|" + st.accounts[account].accountId;
        if (siLoadedFor_ != key) {
            siSel_ = indexOf(kSiKeys, zig::SteamInput::read(st.root, st.accounts[account].accountId, ids.gridId()));
            siLoadedFor_ = key;
        }
        int si = siSel_;
        if (u.selector("ex.si", {c.x, y, c.w, kRowH}, T("extras.si"),
                       {T("extras.si.default"), T("extras.si.enabled"), T("extras.si.disabled")}, si, T("extras.si.help")))
            siSel_ = si;
        y += kRowH + 3;
        if (u.button("ex.siapply", {c.x, y, 200, 18}, T("extras.si.apply"), T("extras.si.apply.help"))) {
            auto r = zig::SteamInput::write(st.root, st.accounts[account].accountId, ids.gridId(), kSiKeys[siSel_]);
            toast = T(r.msgId.c_str());
            if (r.ok) {
                cfg.extras.steamInput = kSiKeys[siSel_];
                result = std::max(result, 1);
                theme().sndConfirm();
            } else {
                u.error();
            }
            siLoadedFor_.clear();
        }
        y += 24;
    }
    u.textBlock({c.x, y, c.w, c.y + c.h - y}, T("extras.st.info"), Theme::kSmall, pal::textDim);
    if (u.focused().empty()) u.focus("ex.logo");
    return result;
}

// ------------------------------------------------------------------ ABOUT > UPDATES
int Extras::aboutPageSelector(Rect& c) {
    UI& u = ui();
    takeStartPage(aboutPage_);
    u.selector("about.page", {c.x, c.y, c.w, kRowH}, T("extras.about.page"),
               {T("extras.about.page.about"), T("extras.about.page.upd")}, aboutPage_, T("extras.about.page.help"));
    c.y += kRowH + 4;
    c.h -= kRowH + 4;
    return aboutPage_;
}

bool Extras::updatesPage(Rect c, zig::Config& cfg) {
    UI& u = ui();
    bool ch = false;
    int y = c.y;
    theme().text(Tf("extras.upd.version", {zig::kVersion}), c.x, y, Theme::kSmall, pal::emphasis);
    y += 14;
    bool on = cfg.extras.updateCheck;
    if (u.toggle("ex.upd", {c.x, y, c.w, kRowH}, T("extras.upd.toggle"), on, T("extras.upd.toggle.help"))) {
        cfg.extras.updateCheck = on;
        ch = true;
    }
    y += kRowH + 3;
    const std::string& feed = cfg.extras.updateFeed;
    theme().text(feed.empty() ? T("extras.upd.nofeed") : Tf("extras.upd.feed", {feed.substr(0, 60)}), c.x, y, Theme::kSmall,
                 feed.empty() ? pal::textDim : pal::text);
    y += 14;
    bool canCheck = on && !feed.empty() && !busy_;
    if (u.button("ex.updnow", {c.x, y, 200, 20}, busy_ ? T("extras.upd.checking") : T("extras.upd.check"),
                 feed.empty() ? T("extras.upd.nofeed") : T("extras.upd.check.help"))) {
        if (!canCheck) {
            u.error();
        } else {
            if (th_.joinable()) th_.join();
            busy_ = true;
            std::string url = feed;
            th_ = std::thread([this, url] {
                auto r = zig::UpdateCheck::check(url, zig::kVersion);
                std::lock_guard<std::mutex> lk(m_);
                upd_ = r;
                haveUpd_ = true;
                busy_ = false;
            });
        }
    }
    zig::UpdateInfo r;
    bool have;
    {
        std::lock_guard<std::mutex> lk(m_);
        r = upd_;
        have = haveUpd_;
    }
    if (have && r.ok && r.newer && !r.url.empty()) {
        if (u.button("ex.upddl", {c.x + 210, y, 200, 20}, T("extras.upd.open"), T("extras.upd.open.help")))
            SDL_OpenURL(r.url.c_str());   // only opens the page; nothing is downloaded by the launcher
    }
    y += 26;
    if (have) {
        std::string msg = r.ok ? Tf(r.msgId.c_str(), {r.version}) : T(r.msgId.c_str());
        theme().text(msg, c.x, y, Theme::kSmall, r.ok ? (r.newer ? pal::emphasis : pal::ok) : pal::select);
        y += 12;
        if (r.ok && !r.notes.empty()) y += u.textBlock({c.x, y, c.w, 60}, r.notes, Theme::kSmall, pal::text) + 4;
    }
    u.textBlock({c.x, y, c.w, c.y + c.h - y}, T("extras.upd.info"), Theme::kSmall, pal::textDim);
    if (u.focused().empty()) u.focus("ex.upd");
    return ch;
}

}  // namespace zl
