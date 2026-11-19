// Enhancements & accessibility switches (LAUNCHER_PLAN.md §7.9), filled from config.json by main.cpp.
// Every default reproduces the original game; each fix site keeps the original code path.
#pragma once
#include "core/Config.h"

// The live settings for this run. Practice options also disable stats recording.
inline zig::EnhancementsCfg& enh() {
    static zig::EnhancementsCfg e;
    return e;
}
