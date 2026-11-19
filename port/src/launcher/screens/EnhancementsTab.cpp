#include "launcher/TestHooks.h"
#include "launcher/screens/EnhancementsTab.h"

#include <string>
#include <vector>

#include "launcher/Strings.h"
#include "launcher/ui/Theme.h"

using namespace zui;
using lz::T;

namespace zl {
namespace {
constexpr int kRowH = 14, kRowGap = 1;

// Any manual change to a preset-controlled field turns the preset into "custom".
void markCustom(zig::EnhancementsCfg& e) { e.preset = "custom"; }

const std::vector<std::string> kPresetKeys{"original", "recommended", "custom"};
const std::vector<int> kSeeds{0, 1, 42, 1234, 4242, 2012};
const std::vector<std::string> kLogLevels{"info", "debug", "verbose"};

int indexOfStr(const std::vector<std::string>& v, const std::string& s) {
    for (size_t i = 0; i < v.size(); i++) if (v[i] == s) return (int)i;
    return 0;
}
}  // namespace

int EnhancementsTab::pageSelector(Rect& c, const zig::EnhancementsCfg& e) {
    UI& u = ui();
    std::vector<std::string> pages{T("enh.page.game"), T("enh.page.access"), T("enh.page.bugs"), T("enh.page.practice")};
    if (e.devOptions) pages.push_back(T("enh.page.dev"));
    takeStartPage(page_);
    if (page_ >= (int)pages.size()) page_ = 0;
    u.selector("enh.page", {c.x, c.y, c.w, kRowH}, T("enh.page"), pages, page_, T("enh.page.help"));
    c.y += kRowH + 4;
    c.h -= kRowH + 4;
    if (page_ != 0 && u.focused().empty()) u.focus("enh.page");
    return page_;
}

bool EnhancementsTab::frame(Rect c, zig::EnhancementsCfg& e) {
    switch (page_) {
        case 1: return accessibilityPage(c, e);
        case 2: return bugsPage(c, e);
        case 3: return practicePage(c, e);
        case 4: return developerPage(c, e);
        default: return false;
    }
}

bool EnhancementsTab::accessibilityPage(Rect c, zig::EnhancementsCfg& e) {
    UI& u = ui();
    bool ch = false;
    int y = c.y;
    auto row = [&]() { Rect r{c.x, y, c.w, kRowH}; y += kRowH + kRowGap; return r; };
    int pi = indexOfStr(kPresetKeys, e.preset);
    if (u.selector("enh.preset", row(), T("enh.preset"),
                   {T("enh.preset.original"), T("enh.preset.recommended"), T("enh.preset.custom")}, pi,
                   T("enh.preset.help"))) {
        if (pi < 2) e.applyPreset(kPresetKeys[pi]);
        else e.preset = "custom";
        ch = true;
    }
    y += 4;
    if (u.toggle("enh.flash", row(), T("enh.flash"), e.reduceFlashing, T("enh.flash.help"))) { markCustom(e); ch = true; }
    {
        char buf[16];
        snprintf(buf, sizeof buf, "%d%%", (int)(e.screenShake * 100 + 0.5f));
        if (u.slider("enh.shake", row(), T("enh.shake"), e.screenShake, 0.0f, 1.0f, 0.25f, T("enh.shake.help"), buf)) {
            markCustom(e);
            ch = true;
        }
    }
    if (u.toggle("enh.logos", row(), T("enh.logos"), e.skipLogos, T("enh.logos.help"))) { markCustom(e); ch = true; }
    if (u.toggle("enh.snapshot", row(), T("enh.snapshot"), e.showDeathSnapshot, T("enh.snapshot.help"))) { markCustom(e); ch = true; }
    y += 6;
    u.textBlock({c.x, y, c.w, 30}, T("enh.access.note"), Theme::kSmall, pal::btnBorder);
    if (u.focused().empty()) u.focus("enh.page");
    return ch;
}

bool EnhancementsTab::bugsPage(Rect c, zig::EnhancementsCfg& e) {
    UI& u = ui();
    bool ch = false;
    int y = c.y;
    u.textBlock({c.x, y, c.w, 22}, T("enh.bugs.note"), Theme::kSmall, pal::btnBorder);
    y += 22;
    auto row = [&]() { Rect r{c.x, y, c.w, kRowH}; y += kRowH + kRowGap; return r; };
    const std::string fixed = T("enh.fixed"), orig = T("enh.original");
    if (u.toggle("enh.fixpurple", row(), T("enh.fixpurple"), e.fixPurpleDoubleCount, T("enh.fixpurple.help"), fixed, orig)) { markCustom(e); ch = true; }
    if (u.toggle("enh.fixyellow", row(), T("enh.fixyellow"), e.fixYellowAsRed, T("enh.fixyellow.help"), fixed, orig)) { markCustom(e); ch = true; }
    if (u.toggle("enh.fixpounce", row(), T("enh.fixpounce"), e.fixRedPounce, T("enh.fixpounce.help"), fixed, orig)) { markCustom(e); ch = true; }
    if (u.toggle("enh.ja", row(), T("enh.ja"), e.showJaOverlay, T("enh.ja.help"), fixed, orig)) { markCustom(e); ch = true; }
    if (e.fixRedPounce) {
        y += 4;
        u.textBlock({c.x, y, c.w, 20}, T("enh.fixpounce.warn"), Theme::kSmall, pal::pink);
    }
    if (u.focused().empty()) u.focus("enh.page");
    return ch;
}

bool EnhancementsTab::practicePage(Rect c, zig::EnhancementsCfg& e) {
    UI& u = ui();
    bool ch = false;
    int y = c.y;
    u.textBlock({c.x, y, c.w, 22}, T("enh.practice.note"), Theme::kSmall, e.practiceActive() ? pal::pink : pal::btnBorder);
    y += 22;
    auto row = [&]() { Rect r{c.x, y, c.w, kRowH}; y += kRowH + kRowGap; return r; };
    if (u.toggle("enh.god", row(), T("enh.god"), e.godMode, T("enh.god.help"))) ch = true;
    int li = e.startEnemyLevel > 4 ? 4 : e.startEnemyLevel;
    if (u.selector("enh.level", row(), T("enh.level"),
                   {T("enh.level.0"), "1", "2", "3", "4"}, li, T("enh.level.help"))) {
        e.startEnemyLevel = li;
        ch = true;
    }
    y += 10;
    if (u.toggle("enh.devopts", row(), T("enh.devopts"), e.devOptions, T("enh.devopts.help"))) ch = true;
    if (u.focused().empty()) u.focus("enh.page");
    return ch;
}

bool EnhancementsTab::developerPage(Rect c, zig::EnhancementsCfg& e) {
    UI& u = ui();
    bool ch = false;
    int y = c.y;
    auto row = [&]() { Rect r{c.x, y, c.w, kRowH}; y += kRowH + kRowGap; return r; };
    if (u.toggle("enh.overlay", row(), T("enh.overlay"), e.showOverlay, T("enh.overlay.help"))) ch = true;
    if (u.toggle("enh.zones", row(), T("enh.zones"), e.touchZones, T("enh.zones.help"))) ch = true;
    {
        std::vector<std::string> opts;
        int si = 0;
        for (size_t i = 0; i < kSeeds.size(); i++) {
            opts.push_back(kSeeds[i] == 0 ? T("enh.seed.random") : std::to_string(kSeeds[i]));
            if (kSeeds[i] == e.fixedSeed) si = (int)i;
        }
        if (e.fixedSeed != 0 && kSeeds[si] != e.fixedSeed) {  // custom seed from config.json
            opts.push_back(std::to_string(e.fixedSeed));
            si = (int)opts.size() - 1;
        }
        if (u.selector("enh.seed", row(), T("enh.seed"), opts, si, T("enh.seed.help"))) {
            e.fixedSeed = si < (int)kSeeds.size() ? kSeeds[si] : e.fixedSeed;
            ch = true;
        }
    }
    int li = indexOfStr(kLogLevels, e.logLevel);
    if (u.selector("enh.log", row(), T("enh.log"), {"INFO", "DEBUG", "VERBOSE"}, li, T("enh.log.help"))) {
        e.logLevel = kLogLevels[li];
        ch = true;
    }
    if (u.focused().empty()) u.focus("enh.page");
    return ch;
}

bool EnhancementsTab::photosensitivityNotice(zig::EnhancementsCfg& e) {
    if (e.photosensitivityNoticeShown) return false;
    noticeOpen_ = true;
    int r = ui().messageBox("enh.notice", T("enh.notice.title"), T("enh.notice.text"),
                            {T("enh.notice.reduce"), T("btn.ok")});
    if (r < 0) return false;
    noticeOpen_ = false;
    e.photosensitivityNoticeShown = true;
    if (r == 0) {
        e.reduceFlashing = true;
        if (e.preset == "original") e.preset = "custom";
    }
    return true;
}

}  // namespace zl
