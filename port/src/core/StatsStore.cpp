#include "core/StatsStore.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/Config.h"
#include "core/DumpImport.h"
#include "core/Paths.h"
#include "json.hpp"
#include "sqlite/sqlite3.h"

using nlohmann::json;
namespace fs = std::filesystem;

namespace zig {
namespace {

bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream in(fs::u8path(path), std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), {});
    return true;
}

bool writeAtomic(const std::string& path, const std::string& data) {
    std::string tmp = path + ".tmp";
    {
        std::ofstream o(fs::u8path(tmp), std::ios::binary | std::ios::trunc);
        if (!o) return false;
        o << data;
        if (!o) return false;
    }
    std::error_code ec;
    fs::rename(fs::u8path(tmp), fs::u8path(path), ec);
    if (ec) {
        fs::remove(fs::u8path(tmp), ec);
        return false;
    }
    return true;
}

std::string nowStamp() {
    char b[32];
    time_t t = time(nullptr);
    strftime(b, sizeof b, "%Y%m%d-%H%M%S", localtime(&t));
    return b;
}

std::string isoNow() {
    char b[32];
    time_t t = time(nullptr);
    strftime(b, sizeof b, "%Y-%m-%dT%H:%M:%S", localtime(&t));
    return b;
}

ParseResult fail(const char* id, const std::string& text) {
    ParseResult r;
    r.errorId = id;
    r.error = text;
    return r;
}

// Fills stats from a JSON object {attr: number}; unknown keys -> ignored, non-numbers -> error.
bool statsFromObject(const json& o, Stats& out, std::vector<std::string>& ignored, std::string& err) {
    if (!o.is_object()) {
        err = "statistics is not an object";
        return false;
    }
    out = Stats::zero();
    for (auto it = o.begin(); it != o.end(); ++it) {
        if (!isStatAttribute(it.key())) {
            ignored.push_back(it.key());
            continue;
        }
        const json& v = it.value();
        if (v.is_number()) out.v[it.key()] = v.get<double>();
        else if (v.is_boolean()) out.v[it.key()] = v.get<bool>() ? 1 : 0;
        else if (v.is_null()) out.v[it.key()] = 0;
        else {
            err = "value of '" + it.key() + "' is not a number";
            return false;
        }
        double d = out.v[it.key()];
        if (!std::isfinite(d)) {
            err = "value of '" + it.key() + "' is not finite";
            return false;
        }
    }
    return true;
}

// ---- minimal nested XML plist -> json
struct XmlPlist {
    const std::string& s;
    size_t p = 0;
    explicit XmlPlist(const std::string& str) : s(str) {}
    static std::string unescape(std::string t) {
        auto rep = [&](const char* a, const char* b) {
            for (size_t i = 0; (i = t.find(a, i)) != std::string::npos; i += strlen(b)) t.replace(i, strlen(a), b);
        };
        rep("&lt;", "<"); rep("&gt;", ">"); rep("&quot;", "\""); rep("&apos;", "'"); rep("&amp;", "&");
        return t;
    }
    bool nextTag(std::string& tag) {
        for (;;) {
            size_t lt = s.find('<', p);
            if (lt == std::string::npos) return false;
            size_t gt = s.find('>', lt);
            if (gt == std::string::npos) return false;
            tag = s.substr(lt + 1, gt - lt - 1);
            p = gt + 1;
            if (!tag.empty() && (tag[0] == '?' || tag[0] == '!')) continue;  // <?xml ...?>, <!DOCTYPE ...>
            return true;
        }
    }
    std::string text(const std::string& name) {
        size_t e = s.find("</" + name + ">", p);
        if (e == std::string::npos) { p = s.size(); return ""; }
        std::string t = s.substr(p, e - p);
        p = e + name.size() + 3;
        return unescape(t);
    }
    bool value(const std::string& tag, json& out, int depth = 0) {
        if (depth > 32) return false;
        if (tag == "dict") {
            out = json::object();
            std::string t, key;
            while (nextTag(t)) {
                if (t == "/dict") return true;
                if (t == "key") { key = text("key"); continue; }
                json v;
                if (!value(t, v, depth + 1)) return false;
                out[key] = std::move(v);
            }
            return false;
        }
        if (tag == "array") {
            out = json::array();
            std::string t;
            while (nextTag(t)) {
                if (t == "/array") return true;
                json v;
                if (!value(t, v, depth + 1)) return false;
                out.push_back(std::move(v));
            }
            return false;
        }
        if (tag == "dict/") { out = json::object(); return true; }
        if (tag == "array/") { out = json::array(); return true; }
        if (tag == "integer") { std::string t = text("integer"); out = (long long)strtoll(t.c_str(), nullptr, 10); return true; }
        if (tag == "real") { std::string t = text("real"); out = t.empty() ? 0.0 : atof(t.c_str()); return true; }
        if (tag == "string") { out = text("string"); return true; }
        if (tag == "string/") { out = ""; return true; }
        if (tag == "true/") { out = true; return true; }
        if (tag == "false/") { out = false; return true; }
        if (tag == "date") { out = text("date"); return true; }
        if (tag == "data") { text("data"); out = nullptr; return true; }
        return false;
    }
};

bool xmlPlistToJson(const std::vector<uint8_t>& d, json& out) {
    std::string s(d.begin(), d.end());
    if (s.find("<plist") == std::string::npos) return false;
    XmlPlist x(s);
    std::string tag;
    while (x.nextTag(tag)) {
        if (tag.rfind("plist", 0) == 0) continue;
        return x.value(tag, out);
    }
    return false;
}

double clampNonNeg(double d) { return d < 0 ? 0 : d; }

}  // namespace

// ================================================================== attributes

const std::vector<std::string>& statAttributes() {
    static const std::vector<std::string> a = {
        "purpleDeaths", "giantDeaths", "giantKills", "facebookPosts", "lastRedKills", "tweets",
        "lastSecondsPlayed", "lastYellowKills", "bestUfoHits", "blueDeaths", "lastGiantKills", "missedShots",
        "ufoHits", "lastShots", "orangeKills", "bestSeconds", "ufoActiveSeconds", "bestMissedShots",
        "lastBlueKills", "bestKills", "bulletDeaths", "moonExplosions", "averageKills", "yellowKills",
        "lastUfoHits", "lastHeadshots", "purpleKills", "averageShots", "orangeDeaths", "secondsPlayed",
        "ufoBestSeconds", "averageSeconds", "lastOrangeKills", "shots", "bigPurpleDeaths", "blueKills",
        "gamesPlayed", "headShots", "lastMissedShots", "lastKills", "redKills", "lastPurpleKills",
        "bestShots", "kills", "bigPurpleKills", "redDeaths", "yellowDeaths", "lastBigPurpleKills"};
    return a;
}

bool isStatAttribute(const std::string& n) {
    const auto& a = statAttributes();
    return std::find(a.begin(), a.end(), n) != a.end();
}

bool Stats::empty() const {
    for (auto& kv : v)
        if (kv.second != 0) return false;
    return true;
}

Stats Stats::zero() {
    Stats s;
    for (auto& a : statAttributes()) s.v[a] = 0;
    return s;
}

// ================================================================== files

std::string statsFile() { return Paths::data("statistics.txt"); }
std::string defaultsFile() { return Paths::data("defaults.tsv"); }
std::string backupsDir() {
    std::string d = Paths::dataDir() + "backups/";
    makeDirs(d);
    return d;
}

bool loadStats(Stats& out, const std::string& file) {
    out = Stats::zero();
    std::ifstream in(fs::u8path(file.empty() ? statsFile() : file));
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        out.v[line.substr(0, eq)] = atof(line.c_str() + eq + 1);   // same as Statistics::load
    }
    return true;
}

bool saveStats(const Stats& s, const std::string& file) {
    // Same format as Statistics::save(): std::map order, "%.9g".
    std::map<std::string, double> all;
    for (auto& a : statAttributes()) all[a] = s.get(a);
    for (auto& kv : s.v) all[kv.first] = kv.second;   // keep unknown keys the game may have written
    std::string out;
    char buf[64];
    for (auto& [k, v] : all) {
        snprintf(buf, sizeof buf, "%.9g", v);
        out += k + "=" + buf + "\n";
    }
    return writeAtomic(file.empty() ? statsFile() : file, out);
}

bool loadDefaults(std::map<std::string, std::string>& out, const std::string& file) {
    out.clear();
    std::ifstream in(fs::u8path(file.empty() ? defaultsFile() : file));
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto tab = line.find('\t');
        if (tab != std::string::npos) out[line.substr(0, tab)] = line.substr(tab + 1);
    }
    return true;
}

bool saveDefaults(const std::map<std::string, std::string>& d, const std::string& file) {
    std::string out;
    for (auto& [k, v] : d) out += k + "\t" + v + "\n";
    return writeAtomic(file.empty() ? defaultsFile() : file, out);
}

// ================================================================== JSON

StatsSettings currentSettings() {
    StatsSettings s;
    s.present = true;
    loadDefaults(s.defaults);
    const Config& c = Config::get();
    s.controlScheme = c.game.controlScheme;
    s.aimGuide = c.game.aimGuide;
    return s;
}

std::string exportJson(const Stats& s, const StatsSettings& st) {
    json root = json::object();
    root["format"] = "ziggurat-stats";
    root["version"] = 1;
    root["exported"] = isoNow();
    json stats = json::object();
    for (auto& a : statAttributes()) stats[a] = s.get(a);
    root["statistics"] = stats;
    if (st.present) {
        json set = json::object();
        set["control_scheme"] = st.controlScheme;
        set["aim_guide"] = st.aimGuide;
        set["defaults"] = st.defaults;
        root["settings"] = set;
    }
    return root.dump(2) + "\n";
}

ParseResult parseExportJson(const std::string& text) {
    json root = json::parse(text, nullptr, false);
    if (root.is_discarded() || !root.is_object()) return fail("stats.err.json", "Not a valid JSON file.");
    // Accept our export format, or a bare {attr: number} object.
    const json* statsObj = &root;
    if (root.contains("format")) {
        if (root["format"] != "ziggurat-stats") return fail("stats.err.format", "Not a ZiGGURAT records file.");
        if (!root.contains("version") || !root["version"].is_number_integer() || root["version"].get<int>() > 1)
            return fail("stats.err.version", "This records file was made by a newer launcher.");
        if (!root.contains("statistics")) return fail("stats.err.format", "The file has no statistics section.");
        statsObj = &root["statistics"];
    }
    ParseResult r;
    std::string err;
    if (!statsFromObject(*statsObj, r.stats, r.ignored, err)) return fail("stats.err.values", err);
    if (statsObj == &root && r.ignored.size() == root.size()) return fail("stats.err.format", "No known record names in this file.");
    if (root.contains("settings") && root["settings"].is_object()) {
        const json& s = root["settings"];
        r.settings.present = true;
        if (s.contains("control_scheme") && s["control_scheme"].is_string()) {
            std::string cs = s["control_scheme"];
            r.settings.controlScheme = cs == "slingshot" ? "slingshot" : "precision";
        }
        if (s.contains("aim_guide") && s["aim_guide"].is_boolean()) r.settings.aimGuide = s["aim_guide"];
        if (s.contains("defaults") && s["defaults"].is_object())
            for (auto it = s["defaults"].begin(); it != s["defaults"].end(); ++it)
                if (it.value().is_string()) r.settings.defaults[it.key()] = it.value().get<std::string>();
    }
    r.ok = true;
    return r;
}

// ================================================================== touchHLE plist

ParseResult importTouchHlePlist(const std::string& path) {
    std::vector<uint8_t> d;
    if (!readFile(path, d)) return fail("stats.err.read", "Can't read the file.");
    json root;
    std::string text;
    if (binaryPlistToJson(d, text)) root = json::parse(text, nullptr, false);
    else if (!xmlPlistToJson(d, root)) return fail("stats.err.plist", "Not a property list file.");
    if (root.is_discarded() || !root.is_object()) return fail("stats.err.plist", "Not a property list file.");
    // touchHLE CoreData store: {"Statistics": [ {...} ]}; also accept a single dict of attributes.
    const json* obj = nullptr;
    if (root.contains("Statistics") && root["Statistics"].is_array() && !root["Statistics"].empty())
        obj = &root["Statistics"][0];
    else if (root.contains("Statistics") && root["Statistics"].is_object())
        obj = &root["Statistics"];
    else
        obj = &root;
    ParseResult r;
    std::string err;
    if (!statsFromObject(*obj, r.stats, r.ignored, err)) return fail("stats.err.values", err);
    if (obj == &root && r.ignored.size() == root.size())
        return fail("stats.err.nostats", "No ZiGGURAT records found in this file.");
    r.ok = true;
    return r;
}

// ================================================================== CoreData SQLite

ParseResult importCoreDataSqlite(const std::string& path) {
    std::vector<uint8_t> head;
    if (!readFile(path, head)) return fail("stats.err.read", "Can't read the file.");
    if (head.size() < 16 || memcmp(head.data(), "SQLite format 3", 15) != 0)
        return fail("stats.err.encrypted", "This is not a readable database. If it comes from an encrypted iPhone backup, "
                                           "make an unencrypted backup first.");
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return fail("stats.err.sqlite", "Not a readable SQLite database.");
    }
    ParseResult r;
    std::map<std::string, std::string> colToAttr;   // ZKILLS -> kills
    for (auto& a : statAttributes()) {
        std::string c = "Z";
        for (char ch : a) c += (char)toupper((unsigned char)ch);
        colToAttr[c] = a;
    }
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT * FROM ZSTATISTICS ORDER BY Z_PK", -1, &st, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        return fail("stats.err.notable", "This database has no ZiGGURAT records (table ZSTATISTICS missing).");
    }
    int rows = 0;
    r.stats = Stats::zero();
    while (sqlite3_step(st) == SQLITE_ROW) {
        if (++rows > 1) continue;   // the game keeps a single Statistics object
        int n = sqlite3_column_count(st);
        for (int i = 0; i < n; i++) {
            const char* cn = sqlite3_column_name(st, i);
            auto it = colToAttr.find(cn ? cn : "");
            if (it == colToAttr.end()) continue;
            int t = sqlite3_column_type(st, i);
            if (t == SQLITE_INTEGER || t == SQLITE_FLOAT) r.stats.v[it->second] = sqlite3_column_double(st, i);
            else if (t == SQLITE_NULL) r.stats.v[it->second] = 0;
        }
    }
    sqlite3_finalize(st);
    sqlite3_close(db);
    if (rows == 0) return fail("stats.err.nostats", "The database contains no records yet.");
    if (rows > 1) r.ignored.push_back("ZSTATISTICS rows after the first (" + std::to_string(rows - 1) + ")");
    r.ok = true;
    return r;
}

ParseResult importAny(const std::string& pathIn) {
    std::string path = pathIn;
    if (dirExists(path)) {
        path = findSaveInFolder(path);
        if (path.empty()) return fail("stats.err.notfound", "No ZiGGURAT save was found in this folder.");
        if (fs::u8path(path).filename().u8string().find(".plist") == std::string::npos) return importCoreDataSqlite(path);
    }
    std::vector<uint8_t> d;
    if (!readFile(path, d)) return fail("stats.err.read", "Can't read the file.");
    if (d.size() >= 16 && memcmp(d.data(), "SQLite format 3", 15) == 0) return importCoreDataSqlite(path);
    if (d.size() >= 8 && memcmp(d.data(), "bplist00", 8) == 0) return importTouchHlePlist(path);
    std::string head(d.begin(), d.begin() + std::min<size_t>(d.size(), 512));
    if (head.find("<plist") != std::string::npos || head.find("<?xml") != std::string::npos) return importTouchHlePlist(path);
    return parseExportJson(std::string(d.begin(), d.end()));
}

// ================================================================== locating saves

std::string sha1Hex(const std::string& data) {
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    std::string m = data;
    uint64_t bits = (uint64_t)data.size() * 8;
    m += (char)0x80;
    while (m.size() % 64 != 56) m += (char)0;
    for (int i = 7; i >= 0; i--) m += (char)((bits >> (i * 8)) & 0xFF);
    auto rol = [](uint32_t x, int n) { return (x << n) | (x >> (32 - n)); };
    for (size_t off = 0; off < m.size(); off += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = (uint32_t)(uint8_t)m[off + 4 * i] << 24 | (uint32_t)(uint8_t)m[off + 4 * i + 1] << 16 |
                   (uint32_t)(uint8_t)m[off + 4 * i + 2] << 8 | (uint32_t)(uint8_t)m[off + 4 * i + 3];
        for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6; }
            uint32_t t = rol(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rol(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    char out[41];
    for (int i = 0; i < 5; i++) snprintf(out + i * 8, 9, "%08x", h[i]);
    return out;
}

std::string findSaveInFolder(const std::string& dir) {
    std::error_code ec;
    fs::path root = fs::u8path(dir);
    if (fs::is_regular_file(root, ec)) return dir;
    // 1) iTunes/Finder backup (unencrypted): hashed file name.
    const std::string hash = sha1Hex("AppDomain-com.freshuu.ziggurat-Documents/HighScore.sqlite");
    for (fs::path cand : {root / hash.substr(0, 2) / hash, root / hash}) {
        if (fs::is_regular_file(cand, ec)) return cand.u8string();
    }
    // a MobileSync/Backup folder with several device backups: check each device folder
    for (auto& e : fs::directory_iterator(root, fs::directory_options::skip_permission_denied, ec)) {
        if (!e.is_directory(ec)) continue;
        for (fs::path cand : {e.path() / hash.substr(0, 2) / hash, e.path() / hash})
            if (fs::is_regular_file(cand, ec)) return cand.u8string();
    }
    // 2) touchHLE sandbox / copied CoreData store, by name.
    std::string found;
    int bestRank = 99;
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
    for (; it != end && !ec; it.increment(ec)) {
        if (it.depth() > 5) { it.disable_recursion_pending(); continue; }
        std::string n = it->path().filename().u8string();
        int rank = n == "HighScore.sqlite" ? 1 : n == "HighScore.sqlite.touchHLE.plist" ? 2 : 99;
        if (rank < bestRank && it->is_regular_file(ec)) { bestRank = rank; found = it->path().u8string(); }
    }
    return found;
}

std::vector<std::string> iosBackupRoots() {
    std::vector<std::string> out;
    auto add = [&](const std::string& p) { if (!p.empty() && dirExists(p)) out.push_back(p); };
#ifdef _WIN32
    if (const char* a = SDL_getenv("APPDATA")) add(std::string(a) + "/Apple Computer/MobileSync/Backup");
    if (const char* u = SDL_getenv("USERPROFILE")) add(std::string(u) + "/Apple/MobileSync/Backup");
#elif defined(__APPLE__)
    if (const char* h = SDL_getenv("HOME")) add(std::string(h) + "/Library/Application Support/MobileSync/Backup");
#endif
    return out;
}

// ================================================================== merge

Stats mergeStats(const Stats& a, const Stats& b, MergePolicy p) {
    if (p == MergePolicy::Replace) {
        Stats r = Stats::zero();
        for (auto& kv : b.v) r.v[kv.first] = kv.second;
        return r;
    }
    Stats r = Stats::zero();
    double ga = a.get("gamesPlayed"), gb = b.get("gamesPlayed");
    for (auto& k : statAttributes()) {
        double x = a.get(k), y = b.get(k);
        if (k.rfind("last", 0) == 0) r.v[k] = y != 0 || gb > 0 ? y : x;
        else if (k.rfind("average", 0) == 0) r.v[k] = (ga + gb) > 0 ? (x * ga + y * gb) / (ga + gb) : 0;
        else if (k == "bestShots" || k == "bestMissedShots") continue;     // pair, below
        else if (k.rfind("best", 0) == 0 || k == "ufoBestSeconds") r.v[k] = std::max(x, y);
        else r.v[k] = clampNonNeg(x) + clampNonNeg(y);                    // totals
    }
    // bestShots/bestMissedShots belong to the same round (best accuracy row in StatState).
    bool takeB = b.get("bestShots") > a.get("bestShots");
    r.v["bestShots"] = takeB ? b.get("bestShots") : a.get("bestShots");
    r.v["bestMissedShots"] = takeB ? b.get("bestMissedShots") : a.get("bestMissedShots");
    return r;
}

// ================================================================== backups

std::string backupNow(const std::string& reason) {
    Stats s;
    loadStats(s);
    StatsSettings st = currentSettings();
    // stats-YYYYmmdd-HHMMSS-NN.json: fixed-width so name order == time order.
    std::string base = backupsDir() + "stats-" + nowStamp();
    // Counter continues after the highest existing one for this second (never reuses a pruned name).
    int next = 1;
    {
        std::string prefix = fs::u8path(base).filename().u8string() + "-";
        std::error_code ec;
        for (auto& e : fs::directory_iterator(fs::u8path(backupsDir()), ec)) {
            std::string n = e.path().filename().u8string();
            if (n.rfind(prefix, 0) == 0) next = std::max(next, atoi(n.c_str() + prefix.size()) + 1);
        }
    }
    char suf[8];
    snprintf(suf, sizeof suf, "-%02d", std::min(next, 99));
    std::string path = base + suf + ".json";
    json j = json::parse(exportJson(s, st));
    j["backup_reason"] = reason;
    if (!writeAtomic(path, j.dump(2) + "\n")) return "";
    // keep newest 5
    auto all = listBackups();
    for (size_t i = 5; i < all.size(); i++) {
        std::error_code ec;
        fs::remove(fs::u8path(all[i].path), ec);
    }
    SDL_Log("stats: backup %s (%s)", fs::u8path(path).filename().u8string().c_str(), reason.c_str());
    return path;
}

std::vector<BackupInfo> listBackups() {
    std::vector<BackupInfo> v;
    std::error_code ec;
    for (auto& e : fs::directory_iterator(fs::u8path(backupsDir()), ec)) {
        std::string n = e.path().filename().u8string();
        if (n.rfind("stats-", 0) != 0 || e.path().extension() != ".json") continue;
        BackupInfo b;
        b.path = e.path().u8string();
        b.name = n;
        // stats-YYYYmmdd-HHMMSS -> YYYY-mm-dd HH:MM:SS
        if (n.size() >= 21) b.when = n.substr(6, 4) + "-" + n.substr(10, 2) + "-" + n.substr(12, 2) + " " + n.substr(15, 2) + ":" +
                                      n.substr(17, 2) + ":" + n.substr(19, 2);
        std::ifstream in(e.path());
        std::stringstream ss;
        ss << in.rdbuf();
        ParseResult r = parseExportJson(ss.str());
        if (r.ok) {
            b.gamesPlayed = (int)r.stats.get("gamesPlayed");
            b.kills = (int)r.stats.get("kills");
        }
        v.push_back(b);
    }
    std::sort(v.begin(), v.end(), [](const BackupInfo& a, const BackupInfo& b) { return a.name > b.name; });
    return v;
}

bool restoreBackup(const std::string& path, std::string* err) {
    std::ifstream in(fs::u8path(path));
    if (!in) { if (err) *err = "Can't read the backup."; return false; }
    std::stringstream ss;
    ss << in.rdbuf();
    ParseResult r = parseExportJson(ss.str());
    if (!r.ok) { if (err) *err = r.error; return false; }
    return applyImport(r, MergePolicy::Replace, true, err);
}

// ================================================================== high level

bool applyImport(const ParseResult& r, MergePolicy p, bool applySettings, std::string* err) {
    if (!r.ok) { if (err) *err = r.error; return false; }
    if (backupNow("before import").empty()) { if (err) *err = "Could not write a backup; nothing was changed."; return false; }
    Stats cur;
    loadStats(cur);
    Stats out = mergeStats(cur, r.stats, p);
    if (!saveStats(out)) { if (err) *err = "Could not write statistics.txt."; return false; }
    if (applySettings && r.settings.present) {
        std::map<std::string, std::string> d;
        loadDefaults(d);
        for (auto& kv : r.settings.defaults) d[kv.first] = kv.second;
        if (!r.settings.controlScheme.empty()) d["preferredControlScheme"] = r.settings.controlScheme;
        d["aimGuide"] = r.settings.aimGuide ? "on" : "off";
        saveDefaults(d);
        Config& c = Config::get();
        if (!r.settings.controlScheme.empty()) c.game.controlScheme = r.settings.controlScheme;
        c.game.aimGuide = r.settings.aimGuide;
        c.save();
    }
    return true;
}

bool resetStats(std::string* err) {
    if (backupNow("before reset").empty()) { if (err) *err = "Could not write a backup; nothing was changed."; return false; }
    if (!saveStats(Stats::zero())) { if (err) *err = "Could not write statistics.txt."; return false; }
    return true;
}

std::string statsSummary(const Stats& s) {
    char b[200];
    snprintf(b, sizeof b, "games=%d kills=%d shots=%d headshots=%d best_kills=%d best_seconds=%d universe_endings=%d",
             (int)s.get("gamesPlayed"), (int)s.get("kills"), (int)s.get("shots"), (int)s.get("headShots"),
             (int)s.get("bestKills"), (int)s.get("bestSeconds"), (int)s.get("moonExplosions"));
    return b;
}

}  // namespace zig
