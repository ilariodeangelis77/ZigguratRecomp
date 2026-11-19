// ZiGGURAT launcher (unofficial) — entry point.
//   ZigguratLauncher [--play] [--fullscreen] [--portable] [--reimport] [--from-steam] [--scale N] [--tab N]
//                    [--gallery ...] [--write-strings file.json]
// Test-only: --wizard-dump <path> (simulates a drop in the wizard), --fallback,
//            --shots "ms:file.bmp;ms:file.bmp" (save canvas screenshots, exit after the last one),
//            env ZIGGURAT_UI_SCRIPT (scripted input, see ui/Input.h).
#include <SDL3/SDL_main.h>

#include <cstdlib>
#include <cstring>
#include <string>

#include "core/Log.h"
#include "core/TestHooks.h"
#include "core/Paths.h"
#include "launcher/App.h"
#include "launcher/Strings.h"

#if ZIGGURAT_TEST_HOOKS
int runGallery(int argc, char** argv);
int runGlyphTest(const std::string& assetsDir, const std::string& report, const std::string& sample);
int runWidthTest(const std::string& assetsDir, const std::string& report);
#endif

int main(int argc, char** argv) {
    zl::Options o;
    bool gallery = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--portable") zig::Paths::forcePortable(true);
#if ZIGGURAT_TEST_HOOKS
        // Developer/test flags: compiled out of release builds (-DZIGGURAT_TEST_HOOKS=OFF).
        else if (a == "--gallery") gallery = true;
        else if (a == "--steam-root" && i + 1 < argc) o.steamRoot = argv[++i];
        else if (a == "--fallback") o.fallbackTheme = true;
        else if (a == "--page" && i + 1 < argc) o.startPage = atoi(argv[++i]);
        else if (a == "--wizard-dump" && i + 1 < argc) o.wizardDump = argv[++i];
        else if (a == "--write-strings" && i + 1 < argc) return lz::writeEnglish(argv[++i]) ? 0 : 1;
        else if (a == "--width-test" && i + 2 < argc) return runWidthTest(argv[i + 1], argv[i + 2]);
        else if (a == "--glyph-test" && i + 2 < argc) {
            std::string assets = argv[i + 1], report = argv[i + 2];
            std::string sample = (i + 3 < argc && argv[i + 3][0] != '-') ? argv[i + 3] : std::string();
            return runGlyphTest(assets, report, sample);
        }
        else if (a == "--shots" && i + 1 < argc) {
            std::string spec = argv[++i];
            size_t p = 0;
            while (p < spec.size()) {
                size_t e = spec.find(';', p);
                std::string item = spec.substr(p, e == std::string::npos ? std::string::npos : e - p);
                size_t c = item.find(':');
                if (c != std::string::npos) o.shots.push_back({atoi(item.substr(0, c).c_str()), item.substr(c + 1)});
                if (e == std::string::npos) break;
                p = e + 1;
            }
        }
#endif
        else if (a == "--play") o.play = true;
        else if (a == "--fullscreen") o.fullscreen = true;
        else if (a == "--reimport") o.reimport = true;
        else if (a == "--from-steam") o.fromSteam = true;
        else if (a == "--scale" && i + 1 < argc) o.scale = atoi(argv[++i]);
        else if (a == "--tab" && i + 1 < argc) o.startTab = atoi(argv[++i]);
    }
    zig::Log::init("launcher");
#if ZIGGURAT_TEST_HOOKS
    if (gallery) {
        int rc = runGallery(argc, argv);
        return rc;
    }
#else
    (void)gallery;
#endif
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS | SDL_INIT_AUDIO)) return 1;
    zl::App app;
    int rc = app.run(o);
    SDL_Quit();
    return rc;
}
