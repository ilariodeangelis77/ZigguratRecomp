#include "launcher/Launch.h"

#include <SDL3/SDL.h>

#include "core/Process.h"

#include <algorithm>
#include <filesystem>

#include "core/Paths.h"

namespace fs = std::filesystem;

namespace zl {

GameProcess::~GameProcess() {
    if (waiter_.joinable()) waiter_.detach();  // game keeps running if the launcher closes
}

std::string GameProcess::gameExe() {
#if defined(_WIN32)
    return zig::Paths::exeDir() + "ziggurat.exe";
#else
    return zig::Paths::exeDir() + "ziggurat";
#endif
}

bool GameProcess::start(const std::vector<std::string>& extraArgs, std::string& error) {
    if (running_) { error = "already running"; return false; }
    if (waiter_.joinable()) waiter_.join();
    std::string exe = gameExe();
    if (!zig::fileExists(exe)) { error = "game executable not found next to the launcher"; return false; }
    std::vector<std::string> args{exe};
    if (zig::Paths::portable()) args.push_back("--portable");
    args.insert(args.end(), extraArgs.begin(), extraArgs.end());
    std::vector<const char*> argv;
    for (auto& a : args) argv.push_back(a.c_str());
    argv.push_back(nullptr);
    SDL_Process* p = zig::spawnProcess(argv.data());
    if (!p) { error = SDL_GetError(); return false; }
    std::string line;
    for (auto& a : args) line += a + " ";
    SDL_Log("launcher: started %s", line.c_str());
    running_ = true;
    finished_ = false;
    waiter_ = std::thread([this, p] {
        int code = 0;
        SDL_WaitProcess(p, true, &code);
        SDL_DestroyProcess(p);
        exitCode_ = code;
        running_ = false;
        finished_ = true;
        SDL_Log("launcher: game exited with code %d", code);
    });
    return true;
}

bool GameProcess::finished() {
    bool f = finished_.exchange(false);
    if (f && waiter_.joinable()) waiter_.join();
    return f;
}

std::string newestGameLog() {
    std::error_code ec;
    fs::path best;
    fs::file_time_type bt{};
    for (auto& e : fs::directory_iterator(fs::u8path(zig::Paths::logsDir()), ec)) {
        auto n = e.path().filename().u8string();
        if (n.rfind("game-", 0) != 0) continue;
        auto t = e.last_write_time(ec);
        if (best.empty() || t > bt) { best = e.path(); bt = t; }
    }
    return best.empty() ? "" : best.u8string();
}

}  // namespace zl
