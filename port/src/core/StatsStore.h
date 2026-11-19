// zigcore: player records ("Statistics", the original CoreData entity) and game settings, for the
// launcher DATA tab and the zigstats CLI (LAUNCHER_PLAN.md §7.8).
//
// Storage is the game's own file: <data>/statistics.txt, "key=value" per line, written with "%.9g"
// (see port/src/game/Shared.cpp). Settings live in <data>/defaults.tsv (NSUserDefaults replacement,
// "key<TAB>value") and config.json.
#pragma once
#include <map>
#include <string>
#include <vector>

namespace zig {

// All attributes of the CoreData model, same list/order as game/Shared.cpp kAttributes.
const std::vector<std::string>& statAttributes();
bool isStatAttribute(const std::string& name);

struct Stats {
    std::map<std::string, double> v;  // missing attribute = 0 (CoreData nil)
    double get(const std::string& k) const {
        auto it = v.find(k);
        return it == v.end() ? 0.0 : it->second;
    }
    bool empty() const;               // every attribute == 0
    static Stats zero();
};

struct StatsSettings {
    bool present = false;
    std::string controlScheme;        // "precision" | "slingshot" | ""
    bool aimGuide = false;
    std::map<std::string, std::string> defaults;   // raw NSUserDefaults keys (defaults.tsv)
};

// ---- files
std::string statsFile();              // <data>/statistics.txt
std::string defaultsFile();           // <data>/defaults.tsv
std::string backupsDir();             // <data>/backups/
bool loadStats(Stats& out, const std::string& file = "");
bool saveStats(const Stats& s, const std::string& file = "");   // atomic, game-compatible format
bool loadDefaults(std::map<std::string, std::string>& out, const std::string& file = "");
bool saveDefaults(const std::map<std::string, std::string>& d, const std::string& file = "");

// ---- JSON export / import ("ziggurat-stats" v1)
std::string exportJson(const Stats& s, const StatsSettings& settings);
StatsSettings currentSettings();      // from defaults.tsv + config.json
struct ParseResult {
    bool ok = false;
    std::string error;                // English, for logs / CLI
    std::string errorId;              // launcher string id (stats.err.*)
    Stats stats;
    StatsSettings settings;
    std::vector<std::string> ignored; // unknown keys that were skipped
};
ParseResult parseExportJson(const std::string& text);

// ---- importers of the original game's save data
// touchHLE sandbox plist (binary or XML): {"Statistics": [ {attr: number, ...} ]}
ParseResult importTouchHlePlist(const std::string& path);
// iOS CoreData store HighScore.sqlite (table ZSTATISTICS, columns Z<UPPERCASE ATTR>).
ParseResult importCoreDataSqlite(const std::string& path);
// Detects the kind of file (.json / plist / sqlite) by content and calls the right parser.
ParseResult importAny(const std::string& path);

// Finds the original save inside a folder: an unencrypted iTunes/Finder backup folder
// (file stored as <sha1[0:2]>/<sha1> or <sha1> of "AppDomain-com.freshuu.ziggurat-Documents/HighScore.sqlite"),
// a touchHLE sandbox (HighScore.sqlite.touchHLE.plist), or a copied HighScore.sqlite (searched depth <= 5).
// Returns "" if nothing was found.
std::string findSaveInFolder(const std::string& dir);
// Default locations of iTunes/Finder backups on this OS (existing folders only).
std::vector<std::string> iosBackupRoots();
std::string sha1Hex(const std::string& data);

// ---- merge
enum class MergePolicy { Replace, KeepBest };
// KeepBest: totals summed, best* = max (bestShots/bestMissedShots taken as a pair), last* from `in`,
// averages recomputed weighted by gamesPlayed.
Stats mergeStats(const Stats& current, const Stats& in, MergePolicy p);

// ---- backups (<data>/backups/stats-YYYYmmdd-HHMMSS[-n].json, newest 5 kept)
std::string backupNow(const std::string& reason);   // returns file path or "" on failure
struct BackupInfo { std::string path, name, when; int gamesPlayed = 0, kills = 0; };
std::vector<BackupInfo> listBackups();               // newest first
bool restoreBackup(const std::string& path, std::string* err = nullptr);   // backs up current first

// ---- high-level operations used by launcher + CLI (each makes a backup first)
bool applyImport(const ParseResult& r, MergePolicy p, bool applySettings, std::string* err = nullptr);
bool resetStats(std::string* err = nullptr);

// Short human summary for support info: "games=8 kills=41 shots=90 best_kills=14".
std::string statsSummary(const Stats& s);

}  // namespace zig
