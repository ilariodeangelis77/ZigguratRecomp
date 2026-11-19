// Starting the game process and watching it (LAUNCHER_PLAN.md §3, §7.10).
#pragma once
#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace zl {

class GameProcess {
public:
    ~GameProcess();
    // Starts <exeDir>/ziggurat(.exe) with extra args. Returns false (and sets error) on failure.
    bool start(const std::vector<std::string>& extraArgs, std::string& error);
    bool running() const { return running_; }
    // Valid once finished() returned true; resets the finished flag.
    bool finished();
    int exitCode() const { return exitCode_; }
    static std::string gameExe();

private:
    std::thread waiter_;
    std::atomic<bool> running_{false}, finished_{false};
    int exitCode_ = 0;
};

// Newest log file written by the game (logs/game-*.log), or "".
std::string newestGameLog();

}  // namespace zl
