// DATA tab pages YOUR RECORDS and SAVES (LAUNCHER_PLAN.md §7.8). Page GAME FILES stays in App.
#pragma once
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "core/GameText.h"
#include "core/StatsStore.h"
#include "launcher/FileBrowser.h"
#include "launcher/ui/ZigUI.h"

namespace zl {

class DataTab {
public:
    // Draws the page selector row at the top of c, shrinks c below it, returns the current page:
    // 0 = GAME FILES (drawn by App), 1 = YOUR RECORDS, 2 = SAVES.
    int pageSelector(zui::Rect& c);
    // Draws pages 1 and 2. assetsDir = imported game files (for the game's own localized labels).
    void frame(zui::Rect c, const std::string& assetsDir);
    // A dialog or the file browser is open (App must not react to Start/Y meanwhile).
    bool busy() const { return modal_ != Modal::None || browser_.isOpen(); }
    // Records changed (import/restore/reset) since the last call; App refreshes its HOME summary.
    bool consumeChanged() { bool c = changed_; changed_ = false; return c; }
    // Extra lines for "Copy support info": records summary (counts only) + enhancement preset.
    static std::string supportLines();

private:
    enum class Modal { None, Preview, Error, Reset1, Reset2, Restore, Info };
    enum class Pick { None, ImportFile, TouchHle, IosBackup, Export };
    void recordsPage(zui::Rect c, const std::string& assetsDir);
    void savesPage(zui::Rect c);
    void dialogs();
    void loadLabels(const std::string& assetsDir);
    // Game's NSLocalizedString by key ID (core/GameText.h); `fallback` if no game files are imported.
    std::string G(txt::Id id, const char* fallback = "") const;
    void startImport(const std::string& path);
    void doExport(const std::string& path);
    void refreshBackups();
    std::string errorText(const zig::ParseResult& r) const;

    int page_ = 0, section_ = 0, mergeMode_ = 0, backupSel_ = 0;
    bool applySettings_ = true, changed_ = false, backupsLoaded_ = false;
    Modal modal_ = Modal::None;
    std::string modalText_, status_;
    bool statusOk_ = true;
    zig::ParseResult pending_;
    std::string restorePath_;
    std::vector<zig::BackupInfo> backups_;
    // game labels
    std::string labelsLang_, labelsDir_;
    std::map<txt::Id, std::string> labels_;
    // native dialogs (result arrives on another thread)
    std::mutex dlgMutex_;
    std::string dlgResult_;
    Pick dlgKind_ = Pick::None, pendingPick_ = Pick::None;
    FileBrowser browser_;
    Pick browserKind_ = Pick::None;
};

}  // namespace zl
