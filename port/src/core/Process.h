// zigcore: start child processes safely from GUI executables.
// SDL_CreateProcess(args, false) inherits stdin/stdout/stderr; a Windows GUI app (the launcher, built
// with -mwindows) has no console handles, so SDL fails with "DuplicateHandle()". Children started
// here get NUL for all three streams (the game writes its own log file).
#pragma once

struct SDL_Process;

namespace zig {
SDL_Process* spawnProcess(const char* const* args);
}  // namespace zig
