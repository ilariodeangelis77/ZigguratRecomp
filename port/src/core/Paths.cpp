#include "core/Paths.h"

#include <SDL3/SDL.h>

#include <filesystem>

namespace fs = std::filesystem;

namespace zig {
namespace {
int gForcePortable = -1;
std::string withSep(std::string s) {
    if (!s.empty() && s.back() != '/' && s.back() != '\\') s += '/';
    return s;
}
}  // namespace

bool fileExists(const std::string& p) {
    std::error_code ec;
    return fs::is_regular_file(fs::u8path(p), ec);
}
bool dirExists(const std::string& p) {
    std::error_code ec;
    return fs::is_directory(fs::u8path(p), ec);
}
bool makeDirs(const std::string& p) {
    std::error_code ec;
    fs::create_directories(fs::u8path(p), ec);
    return dirExists(p);
}

void Paths::forcePortable(bool on) { gForcePortable = on ? 1 : 0; }

const std::string& Paths::exeDir() {
    static std::string d = withSep(SDL_GetBasePath() ? SDL_GetBasePath() : "./");
    return d;
}

bool Paths::portable() {
    if (gForcePortable >= 0) return gForcePortable == 1;
    static bool p = fileExists(exeDir() + "portable.txt");
    return p;
}

const std::string& Paths::dataDir() {
    static std::string d;
    if (d.empty()) {
        if (portable()) {
            d = exeDir() + "data/";
        } else {
            char* p = SDL_GetPrefPath("", "ZigguratRecomp");
            d = withSep(p ? p : (exeDir() + "data/"));
            SDL_free(p);
        }
        makeDirs(d);
    }
    return d;
}

std::string Paths::data(const std::string& rel) {
    std::string full = dataDir() + rel;
    auto parent = fs::u8path(full).parent_path();
    std::error_code ec;
    fs::create_directories(parent, ec);
    return full;
}

std::string Paths::configFile() { return data("config.json"); }
std::string Paths::defaultAssetsDir() { return dataDir() + "assets"; }
std::string Paths::logsDir() {
    std::string d = dataDir() + "logs/";
    makeDirs(d);
    return d;
}

void Paths::migrateLegacy() {
    if (portable()) return;
    char* old = SDL_GetPrefPath("ActionButton", "ZiGGURAT");
    if (!old) return;
    std::string o = withSep(old);
    SDL_free(old);
    for (const char* f : {"defaults.tsv", "statistics.txt"}) {
        std::string src = o + f, dst = dataDir() + f;
        if (fileExists(src) && !fileExists(dst)) {
            std::error_code ec;
            fs::copy_file(fs::u8path(src), fs::u8path(dst), ec);
            SDL_Log("zigcore: migrated %s from legacy folder", f);
        }
    }
}

}  // namespace zig
