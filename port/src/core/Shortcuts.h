// zigcore: desktop / Start-menu shortcuts (LAUNCHER_PLAN.md §7.7).
// Windows: .lnk via IShellLinkW. Linux: freedesktop .desktop file.
// Test override: env ZIGGURAT_SHORTCUT_DIR redirects both locations into one folder.
#pragma once
#include <string>

namespace zig {

enum class ShortcutPlace { Desktop, StartMenu };

struct Shortcuts {
    static void setDirOverride(const std::string& dir);    // tests: create shortcuts in a sandbox
    static std::string dirFor(ShortcutPlace p);             // resolved folder ("" if unknown)
    static std::string pathFor(ShortcutPlace p);            // full path of our shortcut file
    static bool exists(ShortcutPlace p);
    static bool create(ShortcutPlace p, const std::string& target, const std::string& args,
                       const std::string& workDir, const std::string& iconPath, std::string* err = nullptr);
    static bool remove(ShortcutPlace p);
};

}  // namespace zig
