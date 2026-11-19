#include "core/Log.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>

#include "core/FileIO.h"
#include "core/Paths.h"

namespace fs = std::filesystem;

namespace zig {
namespace {
FILE* gFile = nullptr;
std::string gPath;
SDL_LogOutputFunction gPrev = nullptr;
void* gPrevData = nullptr;

void SDLCALL logFn(void* ud, int category, SDL_LogPriority prio, const char* msg) {
    if (gFile) {
        static const char* names[] = {"", "TRACE", "VERBOSE", "DEBUG", "INFO", "WARN", "ERROR", "CRITICAL"};
        fprintf(gFile, "%.3f [%s] %s\n", SDL_GetTicks() / 1000.0,
                (prio > 0 && prio < SDL_LOG_PRIORITY_COUNT) ? names[prio] : "?", msg);
        fflush(gFile);
    }
    if (gPrev) gPrev(gPrevData, category, prio, msg);
}

void rotate(const std::string& dir, const std::string& program) {
    std::vector<fs::path> files;
    std::error_code ec;
    for (auto& e : fs::directory_iterator(fs::u8path(dir), ec))
        if (e.path().filename().u8string().rfind(program + "-", 0) == 0) files.push_back(e.path());
    std::sort(files.begin(), files.end());
    while (files.size() >= 10) {
        fs::remove(files.front(), ec);
        files.erase(files.begin());
    }
}
}  // namespace

void Log::init(const std::string& program) {
    if (gFile) return;
    std::string dir = Paths::logsDir();
    rotate(dir, program);
    char stamp[32];
    time_t t = time(nullptr);
    strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", localtime(&t));
    gPath = dir + program + "-" + stamp + ".log";
    gFile = fopenU8(gPath, "w");
    SDL_GetLogOutputFunction(&gPrev, &gPrevData);
    SDL_SetLogOutputFunction(logFn, nullptr);
}

const std::string& Log::currentFile() { return gPath; }

std::vector<std::string> Log::tail(const std::string& file, int lines) {
    std::ifstream in(fs::u8path(file));
    std::deque<std::string> q;
    std::string l;
    while (std::getline(in, l)) {
        q.push_back(l);
        if ((int)q.size() > lines) q.pop_front();
    }
    return {q.begin(), q.end()};
}

}  // namespace zig
