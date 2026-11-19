// zigstats: player records CLI (LAUNCHER_PLAN.md §7.8) — export / import / backups / reset.
// Uses the same data folder as the game and the launcher; pass --portable to use <exe dir>/data
// (all tests run on a portable copy, never on the real %APPDATA% data).
//
//   zigstats [--portable] show
//   zigstats [--portable] export <file.json>
//   zigstats [--portable] preview <file>                 (json / touchHLE plist / HighScore.sqlite)
//   zigstats [--portable] import <file> [--keep-best] [--settings]
//   zigstats [--portable] import-touchhle <plist> [--keep-best]
//   zigstats [--portable] import-sqlite <HighScore.sqlite> [--keep-best]
//   zigstats [--portable] backups
//   zigstats [--portable] restore <backup.json>
//   zigstats [--portable] reset --yes
//   zigstats make-test-sqlite <out.sqlite> <stats.json>  (synthetic CoreData store for tests)
#include <SDL3/SDL_main.h>
#include "core/FileIO.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "core/Paths.h"
#include "core/StatsStore.h"
#include "sqlite/sqlite3.h"

using namespace zig;

static void printStats(const Stats& s) {
    for (auto& a : statAttributes()) printf("  %-20s %.9g\n", a.c_str(), s.get(a));
}

static int report(const ParseResult& r) {
    if (!r.ok) {
        fprintf(stderr, "error [%s]: %s\n", r.errorId.c_str(), r.error.c_str());
        return 1;
    }
    for (auto& i : r.ignored) printf("  (ignored: %s)\n", i.c_str());
    return 0;
}

static int makeTestSqlite(const std::string& out, const std::string& jsonFile) {
    std::ifstream in(zig::u8p(jsonFile));
    std::stringstream ss;
    ss << in.rdbuf();
    ParseResult r = parseExportJson(ss.str());
    if (report(r)) return 1;
    remove(out.c_str());
    sqlite3* db = nullptr;
    if (sqlite3_open(out.c_str(), &db) != SQLITE_OK) return 1;
    // Same shape CoreData generates: Z_PK/Z_ENT/Z_OPT + Z<ATTR> (INTEGER for counters, FLOAT for averages/seconds),
    // plus the bookkeeping tables Z_PRIMARYKEY and Z_METADATA.
    std::string create = "CREATE TABLE ZSTATISTICS ( Z_PK INTEGER PRIMARY KEY, Z_ENT INTEGER, Z_OPT INTEGER";
    std::string cols = "Z_PK, Z_ENT, Z_OPT", vals = "1, 1, 3";
    for (auto& a : statAttributes()) {
        std::string c = "Z";
        for (char ch : a) c += (char)toupper((unsigned char)ch);
        bool flt = a.rfind("average", 0) == 0 || a.find("Seconds") != std::string::npos;
        create += ", " + c + (flt ? " FLOAT" : " INTEGER");
        cols += ", " + c;
        char b[64];
        snprintf(b, sizeof b, flt ? "%.17g" : "%.0f", r.stats.get(a));
        vals += std::string(", ") + b;
    }
    create += " )";
    std::string sql = create + ";" +
        "CREATE TABLE Z_PRIMARYKEY (Z_ENT INTEGER PRIMARY KEY, Z_NAME VARCHAR, Z_SUPER INTEGER, Z_MAX INTEGER);"
        "INSERT INTO Z_PRIMARYKEY VALUES(1,'Statistics',0,1);"
        "CREATE TABLE Z_METADATA (Z_VERSION INTEGER PRIMARY KEY, Z_UUID VARCHAR(255), Z_PLIST BLOB);"
        "INSERT INTO ZSTATISTICS(" + cols + ") VALUES(" + vals + ");";
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK) fprintf(stderr, "sqlite: %s\n", err ? err : "?");
    sqlite3_free(err);
    sqlite3_close(db);
    printf("wrote %s\n", out.c_str());
    return rc == SQLITE_OK ? 0 : 1;
}

int main(int argc, char** argv) {
    std::vector<std::string> a;
    bool keepBest = false, settings = false, yes = false;
    for (int i = 1; i < argc; i++) {
        std::string s = argv[i];
        if (s == "--portable") Paths::forcePortable(true);
        else if (s == "--keep-best") keepBest = true;
        else if (s == "--settings") settings = true;
        else if (s == "--yes") yes = true;
        else a.push_back(s);
    }
    if (a.empty()) {
        fprintf(stderr, "usage: zigstats [--portable] show|export|preview|import|import-touchhle|import-sqlite|backups|restore|reset|make-test-sqlite ...\n");
        return 2;
    }
    const std::string& cmd = a[0];
    MergePolicy pol = keepBest ? MergePolicy::KeepBest : MergePolicy::Replace;
    if (cmd == "make-test-sqlite" && a.size() >= 3) return makeTestSqlite(a[1], a[2]);
    printf("data: %s\n", Paths::dataDir().c_str());
    if (cmd == "show") {
        Stats s;
        if (!loadStats(s)) printf("(no statistics.txt yet: all zero)\n");
        printStats(s);
        printf("summary: %s\n", statsSummary(s).c_str());
        return 0;
    }
    if (cmd == "export" && a.size() >= 2) {
        Stats s;
        loadStats(s);
        std::ofstream o(zig::u8p(a[1]), std::ios::binary);
        o << exportJson(s, currentSettings());
        printf("exported to %s\n", a[1].c_str());
        return o ? 0 : 1;
    }
    if (cmd == "preview" && a.size() >= 2) {
        ParseResult r = importAny(a[1]);
        if (report(r)) return 1;
        printStats(r.stats);
        if (r.settings.present) printf("  settings: scheme=%s aimGuide=%d defaults=%zu\n", r.settings.controlScheme.c_str(),
                                       (int)r.settings.aimGuide, r.settings.defaults.size());
        return 0;
    }
    if ((cmd == "import" || cmd == "import-touchhle" || cmd == "import-sqlite") && a.size() >= 2) {
        ParseResult r = cmd == "import-touchhle" ? importTouchHlePlist(a[1])
                      : cmd == "import-sqlite"   ? importCoreDataSqlite(a[1])
                                                 : importAny(a[1]);
        if (report(r)) return 1;
        std::string err;
        if (!applyImport(r, pol, settings, &err)) {
            fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }
        Stats s;
        loadStats(s);
        printf("imported (%s): %s\n", keepBest ? "keep-best" : "replace", statsSummary(s).c_str());
        return 0;
    }
    if (cmd == "backups") {
        for (auto& b : listBackups()) printf("  %s  %s  games=%d kills=%d\n", b.name.c_str(), b.when.c_str(), b.gamesPlayed, b.kills);
        return 0;
    }
    if (cmd == "restore" && a.size() >= 2) {
        std::string err;
        if (!restoreBackup(a[1], &err)) {
            fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }
        printf("restored %s\n", a[1].c_str());
        return 0;
    }
    if (cmd == "reset") {
        if (!yes) {
            fprintf(stderr, "refusing to reset without --yes\n");
            return 2;
        }
        std::string err;
        if (!resetStats(&err)) {
            fprintf(stderr, "error: %s\n", err.c_str());
            return 1;
        }
        printf("records reset (backup kept)\n");
        return 0;
    }
    fprintf(stderr, "unknown command or missing argument: %s\n", cmd.c_str());
    return 2;
}
