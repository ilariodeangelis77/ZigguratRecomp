// zigcore: developer "reference mode" — run the ORIGINAL game in a locally built touchHLE for side-by-side
// fidelity checks (LAUNCHER_PLAN.md §7.11). Never shown in normal mode.
#pragma once
#include <string>

namespace zig {

class Reference {
public:
    // configured path, else auto-detect <repo>/_ext/touchHLE/touchHLE_zig.exe relative to the launcher
    // (port/build/ -> ../../_ext/touchHLE/), else "".
    static std::string findTouchHLE(const std::string& configured);
    // A Zig2.app folder for touchHLE: the dump itself when it is an .app folder (or contains Payload/Zig2.app),
    // otherwise extracted from the .ipa into <data>/reference/Zig2.app (once). "" + err on failure.
    static std::string appFolder(const std::string& dumpSource, std::string& err);
    // Starts touchHLE (working dir = its folder, args: "<app>" --landscape-left). Detached.
    static bool launch(const std::string& touchhleExe, const std::string& appDir, std::string& err);
};

}  // namespace zig
