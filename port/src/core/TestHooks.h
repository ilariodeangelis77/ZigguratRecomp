// zigcore: environment-variable test hooks (docs/TESTING.md).
// All ZIGGURAT_* / ZUI_* test hooks go through zig::testEnv(). Release builds are configured with
// -DZIGGURAT_TEST_HOOKS=OFF, which compiles every hook out (testEnv always returns nullptr).
// User features that use environment variables (ZIGGURAT_LANG, ZIGGURAT_SEED) do NOT use this.
#pragma once
#include <SDL3/SDL_stdinc.h>

#ifndef ZIGGURAT_TEST_HOOKS
#define ZIGGURAT_TEST_HOOKS 1
#endif

namespace zig {

constexpr bool kTestHooks = ZIGGURAT_TEST_HOOKS != 0;

inline const char* testEnv(const char* name) {
#if ZIGGURAT_TEST_HOOKS
    return SDL_getenv(name);
#else
    (void)name;
    return nullptr;
#endif
}

}  // namespace zig
