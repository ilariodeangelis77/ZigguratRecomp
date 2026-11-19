// zigcore: per-shortcut Steam Input mode (EXPERIMENTAL, LAUNCHER_PLAN.md §7.6).
// Steam keeps it in userdata/<acc>/config/localconfig.vdf (text VDF):
//   "UserLocalConfigStore" { "apps" { "<shortId>" { "UseSteamControllerConfig" "0|2" } } }
// "0" = disabled, "2" = force enabled; "use default" = key absent (as Steam ROM Manager does).
// Source of the key names: Steam ROM Manager's controller-manager.ts (not Valve documentation).
// The file is edited by splicing text, so everything else stays byte-identical. Steam must be closed
// (it rewrites the file on exit). A backup copy is made next to the file before every change.
#pragma once
#include <string>

namespace zig {

struct SteamInputResult {
    bool ok = false;
    std::string msgId;    // launcher string id
    std::string detail;   // English detail for logs
};

class SteamInput {
public:
    // mode: "default" | "enabled" | "disabled"
    static std::string localConfigPath(const std::string& root, const std::string& accountId);
    static std::string read(const std::string& root, const std::string& accountId, const std::string& shortId);
    static SteamInputResult write(const std::string& root, const std::string& accountId, const std::string& shortId,
                                  const std::string& mode);
    // Pure text transformation (unit-tested): returns the new file text, or "" + err on parse failure.
    static std::string apply(const std::string& text, const std::string& shortId, const std::string& mode,
                             std::string& err);
    static std::string get(const std::string& text, const std::string& shortId);  // "default"/"enabled"/"disabled"
};

}  // namespace zig
