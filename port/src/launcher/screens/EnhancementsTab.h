// GAME tab pages ACCESSIBILITY / ORIGINAL BUGS / PRACTICE / DEVELOPER (LAUNCHER_PLAN.md §7.9).
// Page 0 (GAME) stays in App::tabGame. All defaults reproduce the original game.
#pragma once
#include "core/Config.h"
#include "launcher/ui/ZigUI.h"

namespace zl {

class EnhancementsTab {
public:
    // Draws the page selector row at the top of c, shrinks c below it, returns the current page:
    // 0 = GAME (drawn by App), 1 = ACCESSIBILITY, 2 = ORIGINAL BUGS, 3 = PRACTICE, 4 = DEVELOPER.
    int pageSelector(zui::Rect& c, const zig::EnhancementsCfg& e);
    // Draws pages 1..4; returns true when a setting changed (caller saves config.json).
    bool frame(zui::Rect c, zig::EnhancementsCfg& e);
    // One-time photosensitivity notice (first launcher start). Returns true when it changed config.
    bool photosensitivityNotice(zig::EnhancementsCfg& e);
    bool noticeOpen() const { return noticeOpen_; }
    int page() const { return page_; }

private:
    bool accessibilityPage(zui::Rect c, zig::EnhancementsCfg& e);
    bool bugsPage(zui::Rect c, zig::EnhancementsCfg& e);
    bool practicePage(zui::Rect c, zig::EnhancementsCfg& e);
    bool developerPage(zui::Rect c, zig::EnhancementsCfg& e);
    int page_ = 0;
    bool noticeOpen_ = false;
};

}  // namespace zl
