// zigcore: data locations shared by the game and the launcher.
//   normal:   SDL pref path  (%APPDATA%\ZigguratRecomp\ , ~/.local/share/ZigguratRecomp/)
//   portable: <exe dir>/data/  when a file named portable.txt sits next to the executable
#pragma once
#include <string>

namespace zig {

struct Paths {
    static const std::string& exeDir();     // with trailing separator
    static const std::string& dataDir();    // with trailing separator, created on demand
    static bool portable();
    static std::string data(const std::string& rel);   // dataDir() + rel (creates parent dirs)
    static std::string configFile();                   // data("config.json")
    static std::string defaultAssetsDir();             // data("assets")
    static std::string logsDir();                      // data("logs/")
    // One-time migration from the old "ActionButton/ZiGGURAT" pref folder.
    static void migrateLegacy();
    // Overrides (tests / --portable): call before first use.
    static void forcePortable(bool on);
};

bool fileExists(const std::string& path);
bool dirExists(const std::string& path);
bool makeDirs(const std::string& path);

}  // namespace zig
