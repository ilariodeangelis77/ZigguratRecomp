// zigcore: Steam library artwork generated on the user's machine from their imported game assets
// (nothing is bundled). Pixel art is composed at low resolution and upscaled nearest-neighbour.
//   portrait.png 600x900, wide.png 920x430, hero.png 1920x620 (no logo), logo.png 768x170, icon.png 256x256
#pragma once
#include <string>
#include <vector>

namespace zig {

struct ArtworkResult {
    bool ok = false;
    std::string error;
    std::vector<std::string> files;   // absolute paths written
};

struct ArtworkGen {
    static ArtworkResult generate(const std::string& assetsDir, const std::string& outDir);
    static const char* const kFiles[5];
};

}  // namespace zig
