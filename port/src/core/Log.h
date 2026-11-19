// zigcore: tee SDL_Log output into <data>/logs/<program>-<timestamp>.log (keeps the newest 10).
#pragma once
#include <string>
#include <vector>

namespace zig {
struct Log {
    static void init(const std::string& program);  // e.g. "game", "launcher"
    static const std::string& currentFile();
    static std::vector<std::string> tail(const std::string& file, int lines);
};
}  // namespace zig
