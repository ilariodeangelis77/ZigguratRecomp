// Optional extras (LAUNCHER_PLAN.md §7.6 / §7.11):
//   DATA  > MODS & EXTRAS : loose-file overrides; developer reference mode (original game in touchHLE)
//   STEAM > EXPERIMENTAL  : logo position JSON, Steam Input mode (localconfig.vdf)
//   ABOUT > UPDATES       : version, opt-in update check (never downloads anything)
#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include "core/Config.h"
#include "core/Steam.h"
#include "core/UpdateCheck.h"
#include "launcher/ui/ZigUI.h"

namespace zl {

class Extras {
public:
    ~Extras();
    // DATA tab page 3. Returns true when config changed.
    bool dataPage(zui::Rect c, zig::Config& cfg);
    // STEAM tab: page selector at the top (shrinks c). 0 = LIBRARY (App), 1 = EXPERIMENTAL.
    int steamPageSelector(zui::Rect& c);
    // STEAM > EXPERIMENTAL. Returns: 0 nothing, 1 config changed, 2 "apply to Steam" (App runs Add/Update).
    int steamPage(zui::Rect c, zig::Config& cfg, const zig::SteamStatus& st, int account, const zig::ShortcutSpec& spec,
                  bool added);
    // ABOUT tab: page selector (shrinks c). 0 = ABOUT (App), 1 = UPDATES.
    int aboutPageSelector(zui::Rect& c);
    bool updatesPage(zui::Rect c, zig::Config& cfg);
    // Writes/removes <artDir>/logo.json according to cfg (called before Add/Update).
    static void prepareLogoJson(const std::string& artDir, const zig::ExtrasCfg& x);
    static int overrideCount();
    static std::string overridesDir();

    std::string toast;   // App shows and clears it

private:
    int steamPage_ = 0, aboutPage_ = 0;
    // Steam Input selection (applied with a button)
    int siSel_ = -1;
    std::string siLoadedFor_;
    // detected logo json (read-only)
    std::string logoJson_;
    double logoJsonNext_ = 0;
    // update check worker
    std::thread th_;
    std::atomic<bool> busy_{false};
    std::mutex m_;
    zig::UpdateInfo upd_;
    bool haveUpd_ = false;
    // reference mode
    std::string refStatus_;
    int ovCount_ = -1;
    double ovNext_ = 0;
};

}  // namespace zl
