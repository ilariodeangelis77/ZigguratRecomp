#include "launcher/TestHooks.h"
#include "core/TestHooks.h"
#include "launcher/screens/DataTab.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/Config.h"
#include "core/Paths.h"
#include "json.hpp"
#include "launcher/Strings.h"

namespace fs = std::filesystem;
using namespace zui;
using lz::T;
using lz::Tf;

namespace zl {
namespace {

constexpr int kRowH = 14, kRowGap = 1, kLine = 10;

std::string upper(std::string s) {
    for (auto& ch : s) ch = (char)toupper((unsigned char)ch);
    return s;
}

// Replaces the first %i / %d in a (localized) format with n. Never passes translated text to printf.
std::string fmtInt(const std::string& f, long long n) {
    std::string s = f;
    for (const char* tok : {"%i", "%d"}) {
        size_t p = s.find(tok);
        if (p != std::string::npos) return s.replace(p, 2, std::to_string(n));
    }
    return s + " " + std::to_string(n);
}

std::string pct1(double v) {
    char b[32];
    snprintf(b, sizeof b, "%1.1f%%", v);
    return b;
}

std::string userDir(SDL_Folder f) {
    const char* p = SDL_GetUserFolder(f);
    return p ? p : "";
}

std::string fileName(const std::string& p) { return fs::u8path(p).filename().u8string(); }

}  // namespace

// ------------------------------------------------------------------ labels (game's own strings)

void DataTab::loadLabels(const std::string& assetsDir) {
    if (labelsLang_ == lz::language() && labelsDir_ == assetsDir) return;
    labelsLang_ = lz::language();
    labelsDir_ = assetsDir;
    labels_.clear();
    // English first (values equal the keys: NSLocalizedString's fallback), then the language on top.
    for (std::string lang : {std::string("en"), labelsLang_}) {
        std::ifstream in(fs::u8path(assetsDir + "/strings/" + lang + ".json"));
        if (!in) continue;
        std::stringstream ss;
        ss << in.rdbuf();
        auto j = nlohmann::json::parse(ss.str(), nullptr, false);
        if (j.is_object())
            for (auto it = j.begin(); it != j.end(); ++it)
                if (it.value().is_string()) labels_[txt::hash(it.key())] = it.value().get<std::string>();
    }
}

std::string DataTab::G(txt::Id id, const char* fallback) const {
    auto it = labels_.find(id);
    return it != labels_.end() ? it->second : fallback;
}

// ------------------------------------------------------------------ page selector

int DataTab::pageSelector(Rect& c) {
    UI& u = ui();
    takeStartPage(page_);
    if (!browser_.isOpen())
        u.selector("data.page", {c.x, c.y, c.w, kRowH}, T("data.page"),
                   {T("data.page.files"), T("data.page.records"), T("data.page.saves"), T("data.page.extras")}, page_, T("data.page.help"));
    c.y += kRowH + 4;
    c.h -= kRowH + 4;
    if (page_ != 0 && u.focused().empty()) u.focus("data.page");
    return page_;
}

void DataTab::frame(Rect c, const std::string& assetsDir) {
    // native dialog results arrive on another thread
    {
        std::string r;
        Pick k;
        {
            std::lock_guard<std::mutex> lk(dlgMutex_);
            r.swap(dlgResult_);
            k = dlgKind_;
            dlgKind_ = Pick::None;
        }
        if (!r.empty()) {
            if (k == Pick::Export) doExport(r);
            else startImport(r);
        }
    }
    if (browser_.isOpen()) {
        if (browser_.frame()) startImport(browser_.result());
        return;
    }
    loadLabels(assetsDir);
    // Test hook: ZIGGURAT_STATS_IMPORT=<file or folder> starts an import once the SAVES page is shown.
    static bool testImportDone = false;
    if (page_ == 2 && !testImportDone) {
        testImportDone = true;
        if (const char* t = zig::testEnv("ZIGGURAT_STATS_IMPORT")) startImport(t);
    }
    if (page_ == 1) recordsPage(c, assetsDir);
    else if (page_ == 2) savesPage(c);
    dialogs();
}

// ------------------------------------------------------------------ YOUR RECORDS

void DataTab::recordsPage(Rect c, const std::string&) {
    UI& u = ui();
    zig::Stats s;
    zig::loadStats(s);
    u.selector("rec.section", {c.x, c.y, c.w, kRowH}, T("rec.section"),
               {T("rec.sec.totals"), T("rec.sec.best"), T("rec.sec.deaths")}, section_, T("rec.section.help"));
    int y = c.y + kRowH + 3;
    // Labels are the game's own strings from the imported files; without them, the attribute name.
    auto header = [&](txt::Id key) {
        theme().text(G(key), c.x, y, Theme::kSmall, pal::emphasis);
        y += kLine;
    };
    auto row = [&](txt::Id label, const char* attr, const std::string& value, bool hidden = false) {
        theme().text(hidden ? ". . . ?????????" : G(label, attr), c.x + 6, y, Theme::kSmall, pal::text);
        theme().text(value, c.x + c.w, y, Theme::kSmall, pal::logoYellow, Align::Right);
        y += kLine;
    };
    auto n = [&](const char* a) { return (long long)s.get(a); };
    auto num = [&](const char* a) { return std::to_string(n(a)); };
    // same formulas as StatState -updateStrings
    double accuracy = 0, bestAccuracy = 0;
    if ((float)s.get("shots") != 0.0f) accuracy = (double)((1.0f - (float)s.get("missedShots") / (float)s.get("shots")) * 100.0f);
    if ((float)s.get("bestShots") != 0.0f)
        bestAccuracy = (double)((1.0f - (float)s.get("bestMissedShots") / (float)s.get("bestShots")) * 100.0f);
    bool purpleUnknown = n("purpleDeaths") == 0 && n("purpleKills") == 0;
    bool bigPurpleUnknown = n("bigPurpleDeaths") == 0 && n("bigPurpleKills") == 0;

    if (section_ == 0) {
        header(txt::kStatTotal);
        row(txt::kStatKills, "kills", fmtInt(G(txt::kFmtKills, "%i"), n("kills")));
        row(txt::kStatGames, "gamesPlayed", fmtInt(G(txt::kFmtGames, "%i"), n("gamesPlayed")));
        row(txt::kStatSeconds, "secondsPlayed", fmtInt(G(txt::kFmtSeconds, "%i"), n("secondsPlayed")));
        row(txt::kStatShots, "shots", fmtInt(G(txt::kFmtShots, "%i"), n("shots")));
        row(txt::kStatAccuracy, "accuracy", pct1(accuracy));
        row(txt::kStatHeadshots, "headShots", fmtInt(G(txt::kFmtHeadshots, "%i"), n("headShots")));
        row(txt::kStatMoon, "moonExplosions", num("moonExplosions"), n("moonExplosions") == 0);
        y += 2;
        row(txt::kStatBlueKills, "blueKills", num("blueKills"));
        row(txt::kStatYellowKills, "yellowKills", num("yellowKills"));
        row(txt::kStatRedKills, "redKills", num("redKills"));
        row(txt::kStatOrangeKills, "orangeKills", num("orangeKills"));
        row(txt::kStatGiantKills, "giantKills", num("giantKills"));
        row(txt::kStatPurpleKills, "purpleKills", num("purpleKills"), purpleUnknown);
        row(txt::kStatBigPurpleKills, "bigPurpleKills", num("bigPurpleKills"), bigPurpleUnknown);
    } else if (section_ == 1) {
        header(txt::kStatAverage);
        row(txt::kStatAvgKills, "averageKills", fmtInt(G(txt::kFmtKills, "%i"), n("averageKills")));
        row(txt::kStatAvgSeconds, "averageSeconds", fmtInt(G(txt::kFmtSeconds, "%i"), n("averageSeconds")));
        row(txt::kStatAvgShots, "averageShots", fmtInt(G(txt::kFmtShots, "%i"), n("averageShots")));
        y += 4;
        header(txt::kStatBest);
        row(txt::kStatBestKills, "bestKills", fmtInt(G(txt::kFmtKills, "%i"), n("bestKills")));
        row(txt::kStatBestSeconds, "bestSeconds", fmtInt(G(txt::kFmtSeconds, "%i"), n("bestSeconds")));
        row(txt::kStatBestShots, "bestShots", fmtInt(G(txt::kFmtShots, "%i"), n("bestShots")));
        row(txt::kStatBestAccuracy, "bestAccuracy", pct1(bestAccuracy));
        y += 4;
        row(txt::kStatUfoHits, "ufoHits", num("ufoHits"));
        row(txt::kStatBestUfoHits, "bestUfoHits", num("bestUfoHits"));
        row(txt::kStatUfoKills, "ufoKills", "0");   // the original always shows 0 (no counter exists)
    } else {
        header(txt::kStatDeathsBy);
        row(txt::kStatBulletDeaths, "bulletDeaths", num("bulletDeaths"));
        row(txt::kStatBlueDeaths, "blueDeaths", num("blueDeaths"));
        row(txt::kStatYellowDeaths, "yellowDeaths", num("yellowDeaths"));
        row(txt::kStatRedDeaths, "redDeaths", num("redDeaths"));
        row(txt::kStatOrangeDeaths, "orangeDeaths", num("orangeDeaths"));
        row(txt::kStatGiantDeaths, "giantDeaths", num("giantDeaths"));
        row(txt::kStatPurpleDeaths, "purpleDeaths", num("purpleDeaths"), purpleUnknown);
        row(txt::kStatBigPurpleDeaths, "bigPurpleDeaths", num("bigPurpleDeaths"), bigPurpleUnknown);
    }
    if (s.empty()) theme().text(T("rec.empty"), c.x + c.w, c.y + kRowH + 3, Theme::kSmall, pal::textDim, Align::Right);
}

// ------------------------------------------------------------------ SAVES

void DataTab::refreshBackups() {
    backups_ = zig::listBackups();
    backupsLoaded_ = true;
    if (backupSel_ >= (int)backups_.size()) backupSel_ = std::max(0, (int)backups_.size() - 1);
}

void DataTab::savesPage(Rect c) {
    UI& u = ui();
    if (!backupsLoaded_) refreshBackups();
    SDL_Window* win = gfx().window();
    auto openFile = [&](Pick kind) {
        static const SDL_DialogFileFilter filters[] = {
            {"ZiGGURAT records (.json, .plist, .sqlite)", "json;plist;sqlite"}, {"All files", "*"}};
        pendingPick_ = kind;
        SDL_ShowOpenFileDialog(
            [](void* ud, const char* const* list, int) {
                auto* self = (DataTab*)ud;
                if (list && list[0]) {
                    std::lock_guard<std::mutex> lk(self->dlgMutex_);
                    self->dlgResult_ = list[0];
                    self->dlgKind_ = self->pendingPick_;
                }
            },
            this, win, filters, 2, nullptr, false);
    };
    auto openFolder = [&](Pick kind, const std::string& start) {
        pendingPick_ = kind;
        SDL_ShowOpenFolderDialog(
            [](void* ud, const char* const* list, int) {
                auto* self = (DataTab*)ud;
                if (list && list[0]) {
                    std::lock_guard<std::mutex> lk(self->dlgMutex_);
                    self->dlgResult_ = list[0];
                    self->dlgKind_ = self->pendingPick_;
                }
            },
            this, win, start.empty() ? nullptr : start.c_str(), false);
    };

    const int bw = 216, gap = 8;
    int y = c.y;
    if (u.button("sv.export", {c.x, y, bw, 20}, T("sv.export"), T("sv.export.help"))) {
        char stamp[32];
        time_t t = time(nullptr);
        strftime(stamp, sizeof stamp, "%Y%m%d", localtime(&t));
        static std::string def;
        std::string docs = userDir(SDL_FOLDER_DOCUMENTS);
        def = (docs.empty() ? zig::Paths::dataDir() : docs + "/") + "ziggurat-records-" + stamp + ".json";
        static const SDL_DialogFileFilter jf[] = {{"ZiGGURAT records (.json)", "json"}};
        pendingPick_ = Pick::Export;
        SDL_ShowSaveFileDialog(
            [](void* ud, const char* const* list, int) {
                auto* self = (DataTab*)ud;
                if (list && list[0]) {
                    std::lock_guard<std::mutex> lk(self->dlgMutex_);
                    self->dlgResult_ = list[0];
                    self->dlgKind_ = Pick::Export;
                }
            },
            this, win, jf, 1, def.c_str());
    }
    if (u.button("sv.import", {c.x + bw + gap, y, bw, 20}, T("sv.import"), T("sv.import.help"))) openFile(Pick::ImportFile);
    y += 24;
    if (u.button("sv.touchhle", {c.x, y, bw, 20}, T("sv.touchhle"), T("sv.touchhle.help"))) openFolder(Pick::TouchHle, "");
    if (u.button("sv.ios", {c.x + bw + gap, y, bw, 20}, T("sv.ios"), T("sv.ios.help"))) {
        auto roots = zig::iosBackupRoots();
        openFolder(Pick::IosBackup, roots.empty() ? "" : roots[0]);
    }
    y += 24;
    if (u.button("sv.browse", {c.x, y, bw, 20}, T("sv.browse"), T("sv.browse.help"))) {
        browserKind_ = Pick::ImportFile;
        browser_.openFiles(userDir(SDL_FOLDER_DOCUMENTS), {".json", ".plist", ".sqlite"}, "sv.browse.title", true);
        u.focus("fb.list");
    }
    if (u.button("sv.reset", {c.x + bw + gap, y, bw, 20}, T("sv.reset"), T("sv.reset.help"))) modal_ = Modal::Reset1;
    y += 26;
    u.selector("sv.mode", {c.x, y, c.w, kRowH}, T("sv.mode"), {T("sv.mode.replace"), T("sv.mode.best")}, mergeMode_,
               T("sv.mode.help"));
    y += kRowH + kRowGap;
    u.toggle("sv.settings", {c.x, y, c.w, kRowH}, T("sv.settings"), applySettings_, T("sv.settings.help"));
    y += kRowH + 5;

    theme().text(T("sv.backups"), c.x, y, Theme::kSmall, pal::emphasis);
    y += kLine;
    std::vector<std::string> items;
    for (auto& b : backups_) items.push_back(b.when + "   " + Tf("sv.backup.item", {std::to_string(b.gamesPlayed), std::to_string(b.kills)}));
    if (items.empty()) {
        theme().text(T("sv.backups.none"), c.x + 6, y + 2, Theme::kSmall, pal::textDim);
    } else {
        int listH = std::min((int)items.size(), 3) * 12 + 4;
        if (u.list("sv.backuplist", {c.x, y, c.w - 110, listH}, items, backupSel_, T("sv.backups.help"), 12) &&
            backupSel_ < (int)backups_.size()) {
            restorePath_ = backups_[backupSel_].path;
            modal_ = Modal::Restore;
        }
        if (u.button("sv.restore", {c.x + c.w - 100, y, 100, 20}, T("sv.restore"), T("sv.restore.help")) &&
            backupSel_ < (int)backups_.size()) {
            restorePath_ = backups_[backupSel_].path;
            modal_ = Modal::Restore;
        }
    }
    if (!status_.empty())
        u.textBlock({c.x, c.y + c.h - 20, c.w, 20}, status_, Theme::kSmall, statusOk_ ? pal::ok : pal::select);
}

// ------------------------------------------------------------------ actions

std::string DataTab::errorText(const zig::ParseResult& r) const {
    return !r.errorId.empty() && lz::has(r.errorId) ? T(r.errorId.c_str()) : r.error;
}

void DataTab::startImport(const std::string& path) {
    pending_ = zig::importAny(path);
    if (!pending_.ok) {
        modalText_ = fileName(path) + "\n" + errorText(pending_);
        modal_ = Modal::Error;
        theme().sndError();
        return;
    }
    zig::Stats cur;
    zig::loadStats(cur);
    auto line = [](const zig::Stats& s) {
        return Tf("sv.preview.line", {std::to_string((long long)s.get("gamesPlayed")), std::to_string((long long)s.get("kills")),
                                      std::to_string((long long)s.get("bestKills")), std::to_string((long long)s.get("bestSeconds"))});
    };
    zig::Stats after = zig::mergeStats(cur, pending_.stats, mergeMode_ ? zig::MergePolicy::KeepBest : zig::MergePolicy::Replace);
    modalText_ = fileName(path) + "\n" + T("sv.preview.file") + " " + line(pending_.stats) + "\n" + T("sv.preview.now") + " " +
                 line(cur) + "\n" + T("sv.preview.after") + " " + line(after) + "\n" +
                 (mergeMode_ ? T("sv.mode.best") : T("sv.mode.replace")) +
                 (pending_.settings.present && applySettings_ ? " + " + T("sv.settings") : "") + "\n" + T("sv.preview.backup");
    modal_ = Modal::Preview;
}

void DataTab::doExport(const std::string& pathIn) {
    std::string path = pathIn;
    if (fs::u8path(path).extension().empty()) path += ".json";
    zig::Stats s;
    zig::loadStats(s);
    std::ofstream o(fs::u8path(path), std::ios::binary | std::ios::trunc);
    o << zig::exportJson(s, zig::currentSettings());
    statusOk_ = (bool)o;
    status_ = statusOk_ ? Tf("sv.exported", {fileName(path)}) : T("sv.export.fail");
    statusOk_ ? theme().sndConfirm() : theme().sndError();
}

void DataTab::dialogs() {
    UI& u = ui();
    switch (modal_) {
        case Modal::None: break;
        case Modal::Error: {
            if (u.messageBox("sv.err", T("sv.err.title"), modalText_, {T("btn.ok")}) >= 0) modal_ = Modal::None;
            break;
        }
        case Modal::Info: {
            if (u.messageBox("sv.info", T("sv.done.title"), modalText_, {T("btn.ok")}) >= 0) modal_ = Modal::None;
            break;
        }
        case Modal::Preview: {
            int r = u.messageBox("sv.preview", T("sv.preview.title"), modalText_, {T("sv.do_import"), T("btn.cancel")});
            if (r == 0) {
                std::string err;
                bool ok = zig::applyImport(pending_, mergeMode_ ? zig::MergePolicy::KeepBest : zig::MergePolicy::Replace,
                                           applySettings_, &err);
                statusOk_ = ok;
                status_ = ok ? T("sv.imported") : err;
                ok ? theme().sndConfirm() : theme().sndError();
                changed_ = changed_ || ok;
                refreshBackups();
                modal_ = Modal::None;
            } else if (r == 1) modal_ = Modal::None;
            break;
        }
        case Modal::Reset1: {
            int r = u.messageBox("sv.reset1", T("sv.reset.title"), T("sv.reset.text1"), {T("sv.reset.do"), T("btn.cancel")});
            if (r == 0) modal_ = Modal::Reset2;
            else if (r == 1) modal_ = Modal::None;
            break;
        }
        case Modal::Reset2: {
            int r = u.messageBox("sv.reset2", T("sv.reset.title"), T("sv.reset.text2"), {T("sv.reset.really"), T("btn.cancel")});
            if (r == 0) {
                std::string err;
                bool ok = zig::resetStats(&err);
                statusOk_ = ok;
                status_ = ok ? T("sv.reset.done") : err;
                ok ? theme().sndConfirm() : theme().sndError();
                changed_ = changed_ || ok;
                refreshBackups();
                modal_ = Modal::None;
            } else if (r == 1) modal_ = Modal::None;
            break;
        }
        case Modal::Restore: {
            int r = u.messageBox("sv.restoreq", T("sv.restore.title"), Tf("sv.restore.text", {fileName(restorePath_)}),
                                 {T("sv.restore"), T("btn.cancel")});
            if (r == 0) {
                std::string err;
                bool ok = zig::restoreBackup(restorePath_, &err);
                statusOk_ = ok;
                status_ = ok ? T("sv.restored") : err;
                ok ? theme().sndConfirm() : theme().sndError();
                changed_ = changed_ || ok;
                refreshBackups();
                modal_ = Modal::None;
            } else if (r == 1) modal_ = Modal::None;
            break;
        }
    }
}

// ------------------------------------------------------------------ support info

std::string DataTab::supportLines() {
    zig::Stats s;
    bool have = zig::loadStats(s);
    std::string out = "Records: " + (have ? zig::statsSummary(s) : std::string("none yet")) + ", backups=" +
                      std::to_string(zig::listBackups().size()) + "\n";
    std::string preset = "original";
    auto j = nlohmann::json::parse(zig::Config::get().raw.empty() ? "{}" : zig::Config::get().raw, nullptr, false);
    if (j.is_object() && j.contains("enhancements") && j["enhancements"].is_object() &&
        j["enhancements"].contains("preset") && j["enhancements"]["preset"].is_string())
        preset = j["enhancements"]["preset"].get<std::string>();
    out += "Enhancements preset: " + preset + "\n";
    return out;
}

}  // namespace zl
