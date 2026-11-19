// zigcore: "Add to Steam" (LAUNCHER_PLAN.md §7.6).
// Adds ZiGGURAT as a non-Steam shortcut in userdata/<account>/config/shortcuts.vdf with generated
// artwork in .../config/grid/. Safety: never writes while Steam runs; every change is preceded by a
// backup set in .../config/ziggurat-backups/<timestamp>/ (last 5 kept) and is undoable.
// Test override: env ZIGGURAT_STEAM_ROOT (or Steam::setRootOverride) points everything at a sandbox.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace zig {

struct SteamAccount {
    std::string accountId;   // SteamID3 account number = userdata folder name
    std::string steamId64;
    std::string persona;     // display only
    bool mostRecent = false;
    bool hasUserdata = false;
};

struct ShortcutIds {
    uint32_t top = 0;        // crc32(exe + name) | 0x80000000 -> grid file id (unsigned decimal)
    int32_t appid = 0;       // value stored in shortcuts.vdf (signed)
    uint64_t bigPicture = 0; // (top << 32) | 0x02000000
    std::string gridId() const { return std::to_string(top); }
};

struct ShortcutSpec {
    std::string appName = "ZiGGURAT";
    std::string exePath;          // unquoted absolute path to the launcher
    std::string startDir;         // unquoted
    std::string iconPath;         // absolute path to a .png/.ico (may be the grid icon)
    std::string launchOptions = "--from-steam";
    std::vector<std::string> tags{"Action", "Arcade"};
};

struct SteamStatus {
    bool found = false, running = false, sandbox = false;
    std::string root;
    std::vector<SteamAccount> accounts;
};

struct EntryStatus {
    bool fileExists = false, parsed = false, added = false;
    int index = -1;               // entry index in "shortcuts"
    ShortcutIds ids;
    std::string exe;              // as stored (quoted)
    int entries = 0;
    bool canUndo = false;
    std::string error;
};

struct SteamResult {
    bool ok = false;
    std::string msgId;            // launcher string id (e.g. "steam.err.running")
    std::string detail;           // English detail for logs
};

class Steam {
public:
    static void setRootOverride(const std::string& root);   // "" = auto detect
    static std::string detectRoot();                       // override > registry/paths
    static bool isSandbox();
    static SteamStatus status();
    static std::vector<SteamAccount> accounts(const std::string& root);
    static void setFakeRunning(bool on);                   // sandbox tests (zigsteam --fake-running)
    static bool isRunning();                               // sandbox: env ZIGGURAT_STEAM_FAKE_RUNNING
    static std::string steamExe(const std::string& root);

    static uint32_t crc32(const std::string& s);
    static ShortcutIds idsFor(const std::string& exeField, const std::string& appName);
    static std::string quote(const std::string& path);     // Steam stores Exe/StartDir in quotes

    static std::string configDir(const std::string& root, const std::string& accountId);
    static std::string shortcutsPath(const std::string& root, const std::string& accountId);
    static std::string gridDir(const std::string& root, const std::string& accountId);

    static EntryStatus entryStatus(const std::string& root, const std::string& accountId, const ShortcutSpec& spec);
    // artworkDir must contain portrait.png wide.png hero.png logo.png icon.png (ArtworkGen output).
    static SteamResult addOrUpdate(const std::string& root, const std::string& accountId, const ShortcutSpec& spec,
                                   const std::string& artworkDir);
    static SteamResult remove(const std::string& root, const std::string& accountId, const ShortcutSpec& spec);
    static SteamResult undo(const std::string& root, const std::string& accountId);

    // User-initiated only (launcher buttons). Never called by tests.
    static bool requestShutdown(const std::string& root);
    static bool start(const std::string& root);

    static std::string launchCommand(const ShortcutSpec& spec);   // for "Copy launch command"
    // Experimental: grid/<id>.json (logo position). Returns the file content or "" (read-only).
    static std::string readLogoJson(const std::string& root, const std::string& accountId, const ShortcutSpec& spec);
};

}  // namespace zig
