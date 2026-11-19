#include "core/Steam.h"
#include "core/TestHooks.h"

#include <SDL3/SDL.h>

#include "core/Process.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>

#include "core/Vdf.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace fs = std::filesystem;

namespace zig {
namespace {

std::string gOverride;
bool gOverrideSet = false;

// logo.json (optional, experimental): logo position, written only when the user picks one.
const char* const kGridSuffix[6][2] = {
    {"portrait.png", "p.png"}, {"wide.png", ".png"}, {"hero.png", "_hero.png"},
    {"logo.png", "_logo.png"}, {"icon.png", "_icon.png"}, {"logo.json", ".json"}};

std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}
std::string unquote(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') return s.substr(1, s.size() - 2);
    return s;
}
std::string normPath(const std::string& p) {
    std::string s = lower(unquote(p));
    for (auto& c : s)
        if (c == '\\') c = '/';
    return s;
}
bool exists(const std::string& p) {
    std::error_code ec;
    return fs::exists(fs::u8path(p), ec);
}

#ifdef _WIN32
std::string regString(HKEY root, const wchar_t* sub, const wchar_t* name) {
    wchar_t buf[1024];
    DWORD size = sizeof buf;
    if (RegGetValueW(root, sub, name, RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, buf, -1, nullptr, 0, nullptr, nullptr);
    std::string out(n > 0 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, buf, -1, out.data(), n, nullptr, nullptr);
    return out;
}
std::wstring widen(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n > 0 ? n - 1 : 0, L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}
#endif

// Replace `dst` with `src` (both on the same volume) as atomically as the OS allows.
bool replaceFile(const std::string& src, const std::string& dst) {
#ifdef _WIN32
    return MoveFileExW(widen(src).c_str(), widen(dst).c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::error_code ec;
    fs::rename(fs::u8path(src), fs::u8path(dst), ec);
    return !ec;
#endif
}

bool writeBytes(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream out(fs::u8path(path), std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write((const char*)data.data(), (std::streamsize)data.size());
    return (bool)out;
}

std::string timestamp() {
    char b[32];
    time_t t = time(nullptr);
    strftime(b, sizeof b, "%Y%m%d-%H%M%S", localtime(&t));
    return b;
}

std::string backupsDir(const std::string& root, const std::string& acc) {
    return Steam::configDir(root, acc) + "/ziggurat-backups";
}

std::vector<std::string> backupSets(const std::string& root, const std::string& acc) {
    std::vector<std::string> v;
    std::error_code ec;
    for (auto& e : fs::directory_iterator(fs::u8path(backupsDir(root, acc)), ec))
        if (e.is_directory()) v.push_back(e.path().u8string());
    std::sort(v.begin(), v.end());
    return v;
}

// Snapshot shortcuts.vdf + the given grid files before a change. Keeps the newest 5 sets.
bool makeBackup(const std::string& root, const std::string& acc, const std::vector<std::string>& gridNames,
                std::string& err) {
    std::string base = backupsDir(root, acc) + "/" + timestamp();
    std::string dir = base;
    for (int i = 2; exists(dir); i++) dir = base + "-" + std::to_string(i);
    std::error_code ec;
    fs::create_directories(fs::u8path(dir + "/grid"), ec);
    if (ec) { err = "cannot create backup folder: " + ec.message(); return false; }
    std::string sc = Steam::shortcutsPath(root, acc);
    if (exists(sc)) {
        fs::copy_file(fs::u8path(sc), fs::u8path(dir + "/shortcuts.vdf"), ec);
        if (ec) { err = "cannot back up shortcuts.vdf: " + ec.message(); return false; }
    } else {
        writeBytes(dir + "/shortcuts.vdf.absent", {});
    }
    std::ofstream touched(fs::u8path(dir + "/touched.txt"));
    std::string grid = Steam::gridDir(root, acc);
    for (auto& g : gridNames) {
        touched << g << '\n';
        if (exists(grid + "/" + g)) fs::copy_file(fs::u8path(grid + "/" + g), fs::u8path(dir + "/grid/" + g), ec);
    }
    auto sets = backupSets(root, acc);
    while (sets.size() > 5) {
        fs::remove_all(fs::u8path(sets.front()), ec);
        sets.erase(sets.begin());
    }
    return true;
}

std::vector<std::string> gridNamesFor(const std::string& id) {
    std::vector<std::string> v;
    for (auto& s : kGridSuffix) v.push_back(id + s[1]);
    return v;
}

bool loadShortcuts(const std::string& path, vdf::BNode& root, std::string& err) {
    if (!exists(path)) { root = vdf::emptyShortcuts(); return true; }
    std::vector<uint8_t> data;
    if (!vdf::readFile(path, data)) { err = "cannot read shortcuts.vdf"; return false; }
    if (data.empty()) { root = vdf::emptyShortcuts(); return true; }
    if (!vdf::parseBinary(data, root, &err)) return false;
    if (!root.find("shortcuts")) root.setMap("shortcuts");
    return true;
}

bool saveShortcuts(const std::string& path, const vdf::BNode& root, std::string& err) {
    std::vector<uint8_t> bytes = vdf::writeBinary(root);
    // Sanity: what we write must parse back.
    vdf::BNode check;
    if (!vdf::parseBinary(bytes, check, &err)) { err = "internal: output does not re-parse: " + err; return false; }
    std::string tmp = path + ".ziggurat-tmp";
    if (!writeBytes(tmp, bytes)) { err = "cannot write temp file"; return false; }
    if (!replaceFile(tmp, path)) {
        std::error_code ec;
        fs::remove(fs::u8path(tmp), ec);
        err = "cannot replace shortcuts.vdf";
        return false;
    }
    return true;
}

int findEntry(const vdf::BNode& sc, const ShortcutSpec& spec) {
    std::string wantExe = normPath(spec.exePath);
    for (size_t i = 0; i < sc.kids.size(); i++) {
        const auto& e = sc.kids[i];
        if (e.type != vdf::BType::Map) continue;
        if (normPath(e.getStr("Exe")) == wantExe) return (int)i;
    }
    for (size_t i = 0; i < sc.kids.size(); i++) {
        const auto& e = sc.kids[i];
        if (e.type == vdf::BType::Map && lower(e.getStr("AppName")) == lower(spec.appName)) return (int)i;
    }
    return -1;
}

void renumber(vdf::BNode& sc) {
    int n = 0;
    for (auto& e : sc.kids)
        if (e.type == vdf::BType::Map) e.key = std::to_string(n++);
}

SteamResult fail(const char* id, const std::string& detail) {
    SDL_Log("steam: %s (%s)", id, detail.c_str());
    return {false, id, detail};
}

}  // namespace

// ------------------------------------------------------------------ detection

void Steam::setRootOverride(const std::string& root) {
    gOverride = root;
    gOverrideSet = !root.empty();
}

bool Steam::isSandbox() {
    if (gOverrideSet) return true;
    const char* e = zig::testEnv("ZIGGURAT_STEAM_ROOT");
    return e && *e;
}

std::string Steam::detectRoot() {
    if (gOverrideSet) return gOverride;
    if (const char* e = zig::testEnv("ZIGGURAT_STEAM_ROOT"); e && *e) return e;
#ifdef _WIN32
    std::string r = regString(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath");
    if (r.empty()) r = regString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath");
    if (r.empty() && exists("C:/Program Files (x86)/Steam")) r = "C:/Program Files (x86)/Steam";
    if (!r.empty() && exists(r)) return r;
    return "";
#else
    const char* home = SDL_getenv("HOME");
    if (!home) return "";
    for (std::string p : {std::string(home) + "/.local/share/Steam", std::string(home) + "/.steam/steam",
                          std::string(home) + "/.var/app/com.valvesoftware.Steam/data/Steam"})
        if (exists(p + "/userdata") || exists(p + "/config/loginusers.vdf")) return p;
    return "";
#endif
}

std::vector<SteamAccount> Steam::accounts(const std::string& root) {
    std::vector<SteamAccount> out;
    std::set<std::string> seen;
    std::ifstream in(fs::u8path(root + "/config/loginusers.vdf"));
    if (in) {
        std::string text((std::istreambuf_iterator<char>(in)), {});
        vdf::TNode t;
        if (vdf::parseText(text, t)) {
            if (const vdf::TNode* users = t.find("users")) {
                for (const auto& u : users->kids) {
                    if (!u.isMap) continue;
                    unsigned long long id64 = strtoull(u.key.c_str(), nullptr, 10);
                    if (id64 < 76561197960265728ULL) continue;
                    SteamAccount a;
                    a.steamId64 = u.key;
                    a.accountId = std::to_string(id64 - 76561197960265728ULL);
                    a.persona = u.get("PersonaName", a.accountId);
                    a.mostRecent = u.get("MostRecent") == "1" || u.get("mostrecent") == "1";
                    a.hasUserdata = exists(root + "/userdata/" + a.accountId);
                    seen.insert(a.accountId);
                    out.push_back(a);
                }
            }
        }
    }
    std::error_code ec;
    for (auto& e : fs::directory_iterator(fs::u8path(root + "/userdata"), ec)) {
        std::string name = e.path().filename().u8string();
        if (!e.is_directory() || name.empty() || !std::all_of(name.begin(), name.end(), ::isdigit) || name == "0") continue;
        if (seen.count(name)) continue;
        SteamAccount a;
        a.accountId = name;
        a.persona = name;
        a.hasUserdata = true;
        out.push_back(a);
    }
    // Only accounts with a userdata folder can hold shortcuts; most recent first.
    out.erase(std::remove_if(out.begin(), out.end(), [](const SteamAccount& a) { return !a.hasUserdata; }), out.end());
    std::stable_sort(out.begin(), out.end(), [](const SteamAccount& a, const SteamAccount& b) { return a.mostRecent > b.mostRecent; });
    return out;
}

static bool gFakeRunning = false;
void Steam::setFakeRunning(bool on) { gFakeRunning = on; }

bool Steam::isRunning() {
    if (isSandbox()) {
        if (gFakeRunning) return true;
        const char* f = zig::testEnv("ZIGGURAT_STEAM_FAKE_RUNNING");
        return f && *f == '1';
    }
#ifdef _WIN32
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof pe;
    bool found = false;
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe))
        if (_wcsicmp(pe.szExeFile, L"steam.exe") == 0) { found = true; break; }
    CloseHandle(snap);
    return found;
#else
    std::error_code ec;
    for (auto& e : fs::directory_iterator("/proc", ec)) {
        std::string pid = e.path().filename().string();
        if (pid.empty() || !std::all_of(pid.begin(), pid.end(), ::isdigit)) continue;
        std::ifstream c(e.path() / "comm");
        std::string comm;
        std::getline(c, comm);
        if (comm == "steam" || comm == "steamwebhelper") return true;
    }
    return false;
#endif
}

SteamStatus Steam::status() {
    SteamStatus s;
    s.root = detectRoot();
    s.sandbox = isSandbox();
    s.found = !s.root.empty();
    if (s.found) {
        s.accounts = accounts(s.root);
        s.running = isRunning();
    }
    return s;
}

std::string Steam::steamExe(const std::string& root) {
#ifdef _WIN32
    return root + "/steam.exe";
#else
    (void)root;
    return "steam";
#endif
}

// ------------------------------------------------------------------ ids

uint32_t Steam::crc32(const std::string& s) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        init = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (unsigned char ch : s) c = table[(c ^ ch) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

ShortcutIds Steam::idsFor(const std::string& exeField, const std::string& appName) {
    ShortcutIds ids;
    ids.top = crc32(exeField + appName) | 0x80000000u;
    ids.appid = (int32_t)ids.top;
    ids.bigPicture = ((uint64_t)ids.top << 32) | 0x02000000u;
    return ids;
}

std::string Steam::quote(const std::string& path) {
    std::string p = path;
#ifdef _WIN32
    for (auto& c : p)
        if (c == '/') c = '\\';
#endif
    return "\"" + p + "\"";
}

std::string Steam::configDir(const std::string& root, const std::string& acc) { return root + "/userdata/" + acc + "/config"; }
std::string Steam::shortcutsPath(const std::string& root, const std::string& acc) { return configDir(root, acc) + "/shortcuts.vdf"; }
std::string Steam::gridDir(const std::string& root, const std::string& acc) { return configDir(root, acc) + "/grid"; }

std::string Steam::readLogoJson(const std::string& root, const std::string& acc, const ShortcutSpec& spec) {
    ShortcutIds ids = idsFor(quote(spec.exePath), spec.appName);
    std::ifstream in(fs::u8path(gridDir(root, acc) + "/" + ids.gridId() + ".json"), std::ios::binary);
    if (!in) return {};
    std::string t((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return t.size() > 2000 ? t.substr(0, 2000) : t;
}

std::string Steam::launchCommand(const ShortcutSpec& spec) { return quote(spec.exePath) + " " + spec.launchOptions; }

// ------------------------------------------------------------------ status / edits

EntryStatus Steam::entryStatus(const std::string& root, const std::string& acc, const ShortcutSpec& spec) {
    EntryStatus st;
    std::string path = shortcutsPath(root, acc);
    st.fileExists = exists(path);
    st.canUndo = !backupSets(root, acc).empty();
    vdf::BNode r;
    if (!loadShortcuts(path, r, st.error)) return st;
    st.parsed = true;
    const vdf::BNode* sc = r.find("shortcuts");
    for (auto& e : sc->kids)
        if (e.type == vdf::BType::Map) st.entries++;
    st.index = findEntry(*sc, spec);
    if (st.index >= 0) {
        const auto& e = sc->kids[st.index];
        st.added = true;
        st.exe = e.getStr("Exe");
        st.ids = idsFor(st.exe, e.getStr("AppName"));
    } else {
        st.ids = idsFor(quote(spec.exePath), spec.appName);
    }
    return st;
}

SteamResult Steam::addOrUpdate(const std::string& root, const std::string& acc, const ShortcutSpec& spec,
                               const std::string& artworkDir) {
    if (isRunning()) return fail("steam.err.running", "Steam is running; close it first");
    if (!exists(root + "/userdata/" + acc)) return fail("steam.err.account", "account folder not found");
    std::string path = shortcutsPath(root, acc), err;
    vdf::BNode r;
    if (!loadShortcuts(path, r, err)) return fail("steam.err.corrupt", err);
    vdf::BNode& sc = *r.find("shortcuts");
    int idx = findEntry(sc, spec);

    std::string exeField = quote(spec.exePath);
    ShortcutIds ids = idsFor(exeField, spec.appName);
    std::vector<std::string> touched = gridNamesFor(ids.gridId());
    std::string oldId;
    if (idx >= 0) {
        const auto& e = sc.kids[idx];
        ShortcutIds old = idsFor(e.getStr("Exe"), e.getStr("AppName"));
        if (old.top != ids.top) {
            oldId = old.gridId();
            for (auto& g : gridNamesFor(oldId)) touched.push_back(g);
        }
    }
    std::error_code ec;
    fs::create_directories(fs::u8path(configDir(root, acc)), ec);
    if (!makeBackup(root, acc, touched, err)) return fail("steam.err.backup", err);

    std::string grid = gridDir(root, acc);
    std::string iconPath = grid + "/" + ids.gridId() + "_icon.png";
#ifdef _WIN32
    for (auto& c : iconPath)
        if (c == '/') c = '\\';
#endif
    vdf::BNode* e;
    if (idx >= 0) {
        e = &sc.kids[idx];
    } else {
        vdf::BNode n;
        n.type = vdf::BType::Map;
        int count = 0;
        for (auto& k : sc.kids)
            if (k.type == vdf::BType::Map) count++;
        n.key = std::to_string(count);
        sc.kids.push_back(n);
        e = &sc.kids.back();
    }
    // Field set and order as Steam writes it for a non-Steam shortcut.
    e->setInt("appid", ids.appid);
    e->setStr("AppName", spec.appName);
    e->setStr("Exe", exeField);
    e->setStr("StartDir", quote(spec.startDir));
    e->setStr("icon", spec.iconPath.empty() ? iconPath : spec.iconPath);
    if (!e->find("ShortcutPath")) e->setStr("ShortcutPath", "");
    e->setStr("LaunchOptions", spec.launchOptions);
    if (!e->find("IsHidden")) e->setInt("IsHidden", 0);
    if (!e->find("AllowDesktopConfig")) e->setInt("AllowDesktopConfig", 1);
    if (!e->find("AllowOverlay")) e->setInt("AllowOverlay", 1);
    if (!e->find("OpenVR")) e->setInt("OpenVR", 0);
    if (!e->find("Devkit")) e->setInt("Devkit", 0);
    if (!e->find("DevkitGameID")) e->setStr("DevkitGameID", "");
    if (!e->find("DevkitOverrideAppID")) e->setInt("DevkitOverrideAppID", 0);
    if (!e->find("LastPlayTime")) e->setInt("LastPlayTime", 0);
    if (!e->find("FlatpakAppID")) e->setStr("FlatpakAppID", "");
    if (!e->find("tags")) {
        vdf::BNode& tags = e->setMap("tags");
        for (size_t i = 0; i < spec.tags.size(); i++) tags.setStr(std::to_string(i), spec.tags[i]);
    }

    if (!saveShortcuts(path, r, err)) return fail("steam.err.write", err);

    // Artwork
    fs::create_directories(fs::u8path(grid), ec);
    if (!oldId.empty())
        for (auto& g : gridNamesFor(oldId)) fs::remove(fs::u8path(grid + "/" + g), ec);
    int copied = 0;
    if (!artworkDir.empty()) {
        for (auto& s : kGridSuffix) {
            std::string src = artworkDir + "/" + s[0];
            if (!exists(src)) continue;
            fs::copy_file(fs::u8path(src), fs::u8path(grid + "/" + ids.gridId() + s[1]), fs::copy_options::overwrite_existing, ec);
            if (!ec) copied++;
        }
    }
    SDL_Log("steam: shortcut %s (entry %d, %d artwork files)", idx >= 0 ? "updated" : "added", idx, copied);
    return {true, idx >= 0 ? "steam.ok.updated" : "steam.ok.added", ""};
}

SteamResult Steam::remove(const std::string& root, const std::string& acc, const ShortcutSpec& spec) {
    if (isRunning()) return fail("steam.err.running", "Steam is running; close it first");
    std::string path = shortcutsPath(root, acc), err;
    vdf::BNode r;
    if (!loadShortcuts(path, r, err)) return fail("steam.err.corrupt", err);
    vdf::BNode& sc = *r.find("shortcuts");
    int idx = findEntry(sc, spec);
    if (idx < 0) return fail("steam.err.notadded", "no ZiGGURAT entry");
    ShortcutIds ids = idsFor(sc.kids[idx].getStr("Exe"), sc.kids[idx].getStr("AppName"));
    if (!makeBackup(root, acc, gridNamesFor(ids.gridId()), err)) return fail("steam.err.backup", err);
    sc.kids.erase(sc.kids.begin() + idx);
    renumber(sc);
    if (!saveShortcuts(path, r, err)) return fail("steam.err.write", err);
    std::error_code ec;
    for (auto& g : gridNamesFor(ids.gridId())) fs::remove(fs::u8path(gridDir(root, acc) + "/" + g), ec);
    SDL_Log("steam: shortcut removed");
    return {true, "steam.ok.removed", ""};
}

SteamResult Steam::undo(const std::string& root, const std::string& acc) {
    if (isRunning()) return fail("steam.err.running", "Steam is running; close it first");
    auto sets = backupSets(root, acc);
    if (sets.empty()) return fail("steam.err.noundo", "no backup");
    std::string set = sets.back(), path = shortcutsPath(root, acc), grid = gridDir(root, acc);
    std::error_code ec;
    if (exists(set + "/shortcuts.vdf")) {
        std::string tmp = path + ".ziggurat-tmp";
        fs::copy_file(fs::u8path(set + "/shortcuts.vdf"), fs::u8path(tmp), fs::copy_options::overwrite_existing, ec);
        if (ec || !replaceFile(tmp, path)) return fail("steam.err.write", "cannot restore shortcuts.vdf");
    } else if (exists(set + "/shortcuts.vdf.absent")) {
        fs::remove(fs::u8path(path), ec);
    }
    std::ifstream touched(fs::u8path(set + "/touched.txt"));
    std::string g;
    while (std::getline(touched, g)) {
        if (g.empty() || g.find('/') != std::string::npos || g.find('\\') != std::string::npos) continue;
        std::string saved = set + "/grid/" + g;
        if (exists(saved)) {
            fs::create_directories(fs::u8path(grid), ec);
            fs::copy_file(fs::u8path(saved), fs::u8path(grid + "/" + g), fs::copy_options::overwrite_existing, ec);
        } else {
            fs::remove(fs::u8path(grid + "/" + g), ec);
        }
    }
    touched.close();
    fs::remove_all(fs::u8path(set), ec);
    SDL_Log("steam: last change undone");
    return {true, "steam.ok.undone", ""};
}

bool Steam::requestShutdown(const std::string& root) {
    if (isSandbox()) { SDL_Log("steam: (sandbox) would ask Steam to shut down"); return true; }
    const char* args[] = {nullptr, "-shutdown", nullptr};
    std::string exe = steamExe(root);
    args[0] = exe.c_str();
    SDL_Process* p = zig::spawnProcess(args);
    if (!p) return false;
    SDL_DestroyProcess(p);
    return true;
}

bool Steam::start(const std::string& root) {
    if (isSandbox()) { SDL_Log("steam: (sandbox) would start Steam"); return true; }
    std::string exe = steamExe(root);
    const char* args[] = {exe.c_str(), nullptr};
    SDL_Process* p = zig::spawnProcess(args);
    if (!p) return SDL_OpenURL("steam://open/main");
    SDL_DestroyProcess(p);
    return true;
}

}  // namespace zig
