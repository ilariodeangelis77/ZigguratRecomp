#include "core/Manifest.h"

namespace zig {

#include "core/ManifestData.inc"

const char* Manifest::version() { return kManifestVersion; }
const char* Manifest::bundleId() { return kManifestBundleId; }
int Manifest::count() { return (int)(sizeof kManifestEntries / sizeof kManifestEntries[0]); }
const ManifestEntry& Manifest::at(int i) { return kManifestEntries[i]; }
const ManifestEntry* Manifest::find(const std::string& path) {
    for (auto& e : kManifestEntries)
        if (path == e.path) return &e;
    return nullptr;
}

}  // namespace zig
