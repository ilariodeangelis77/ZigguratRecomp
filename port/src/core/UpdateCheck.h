// zigcore: optional update check (LAUNCHER_PLAN.md §7.11). Off by default; needs a configured feed URL.
// Fetches a small JSON {"version":"x.y.z","url":"https://...","notes":"..."} and compares versions.
// It never downloads or installs anything: the launcher only offers to open the download page.
#pragma once
#include <string>

namespace zig {

struct UpdateInfo {
    bool ok = false;           // fetch + parse succeeded
    bool newer = false;        // feed version > current
    std::string version, url, notes;
    std::string msgId;         // launcher string id for errors ("extras.upd.err.*")
    std::string detail;        // English detail for logs
};

class UpdateCheck {
public:
    static UpdateInfo check(const std::string& feedUrl, const std::string& currentVersion);  // blocking
    static int compareVersions(const std::string& a, const std::string& b);  // <0, 0, >0 (numeric dot parts)
    static bool parseFeed(const std::string& json, UpdateInfo& out);
    static bool httpGet(const std::string& url, std::string& body, std::string& err);  // Windows only
};

}  // namespace zig
