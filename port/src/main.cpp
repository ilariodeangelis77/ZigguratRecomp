// Engine smoke test: runs the translated flixel-ios FlxGame with a test FlxState.
// NOT the real ZiGGURAT title screen (that comes from the decompiled StartState).
//   ziggurat.exe [--assets DIR] [--shot FRAME out.bmp] [--zoom Z]
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "core/Config.h"
#include "core/TestHooks.h"
#include "core/Log.h"
#include "core/Paths.h"
#include "engine/Flixel.h"
#include "game/Enhancements.h"
#include "game/Gameplay.h"
#include "game/Shared.h"
#include "platform/pad.h"

class EngineTestState : public FlxState {
public:
    FlxSprite* hero = nullptr;
    FlxSprite* reticle = nullptr;
    FlxSprite* fader = nullptr;
    FlxEmitter* emitter = nullptr;
    FlxText* label = nullptr;
    FlxText* info = nullptr;
    float t = 0;

    void create() override {
        setBgColor(0xff202040);
        add((new FlxSprite)->initWithX(-16, -32, "sky_sunset.png"));
        auto* clouds = (new FlxSprite)->initWithX(0, 150, "clouds4_sunset.png");
        clouds->velocity.x = -12;
        add(clouds);
        add((new FlxSprite)->initWithX(-35, 160, "mainZig.png"));
        add((new FlxSprite)->initWithX(48, 20, "ziglogo.png"));

        hero = (new FlxSprite)->initWithX(228, 136, std::string());
        hero->loadGraphic("new_Hero.png", true, false, 24, 24);
        hero->addAnimation("run", {0, 1, 2}, 10, true);
        hero->play("run");
        add(hero);

        reticle = (new FlxSprite)->initWithX(300, 120, "img_Reticle.png");
        reticle->angularVelocity = 90;
        add(reticle);

        fader = (new FlxSprite)->initWithX(150, 120, "img_shield.png");
        add(fader);

        emitter = (new FlxEmitter)->initWithEmitterX(240, 160);
        emitter->createSprites("img_littleExplode.png", 30, 0, true);
        emitter->setXSpeedWithParam1(-80, 80);
        emitter->setYSpeedWithParam1(-160, -40);
        emitter->gravity = 300;
        add(emitter);
        emitter->start(false, 0.05f);

        label = (new FlxText)->initWithX(0, 110, 480, "ENGINE PORT TEST");
        label->setFormatWithParam1(std::string(), 16, 0xffff40, "center", 0x800000);
        add(label);
        info = (new FlxText)->initWithX(8, 290, 464, "tap/click: sound + particles. The quick brown fox jumps over the lazy dog 0123456789");
        info->setFormatWithParam1(std::string(), 8, 0xffffff, "left");
        add(info);

        FlxG::playMusic("Overture.mp3");
    }

    void update() override {
        t += FlxG::elapsed();
        fader->setAlpha(0.5f + 0.5f * sinf(t * 3));
        if (FlxG::touches()->touchesBegan) {
            CGPoint p = FlxG::touches()->touchPoint();
            FlxG::play("shoot.wav");
            emitter->x = p.x;
            emitter->y = p.y;
            FlxG::quake()->start(0.01f, 0.2f);
        }
        if (FlxG::touches()->touching) {
            CGPoint p = FlxG::touches()->touchPoint();
            reticle->setX(p.x - reticle->getWidth() / 2);
            reticle->setY(p.y - reticle->getHeight() / 2);
        }
        FlxState::update();
    }
};

// ENHANCEMENT developer overlay: frame-time / entity counters and the touch zones of the scheme.
static int countExisting(FlxGroup* g) {
    int n = 0;
    if (g) for (auto* m : g->members) if (m && m->exists) n++;
    return n;
}
static void drawDebugOverlay() {
    const auto& E = enh();
    auto* ps = dynamic_cast<PlayState*>(FlxG::state());
    char buf[96];
    if (E.showOverlay) {
        static double last = plat::nowSeconds(), fps = 0;
        double now = plat::nowSeconds(), dt = now - last;
        last = now;
        if (dt > 0) fps = fps * 0.9 + (1.0 / dt) * 0.1;
        snprintf(buf, sizeof buf, "FPS %.0f  DT %.3f", fps, FlxG::elapsed());
        plat::debugText(2, 2, buf, 0xFFCCFF00);
        if (ps) {
            int aliens = countExisting(ps->_enemies) + countExisting(ps->_yellowEnemies) + countExisting(ps->_redEnemies) +
                         countExisting(ps->_orangeEnemies) + countExisting(ps->_purpleEnemies) +
                         countExisting(ps->_bigEnemies) + countExisting(ps->_bigPurpleEnemies);
            snprintf(buf, sizeof buf, "ALIENS %d EBUL %d PBUL %d LVL %d/%d", aliens, countExisting(ps->_enemyBullets),
                     countExisting(ps->_playerBullets), ps->_currentEnemyLvl, ps->_currentBgLvl);
            plat::debugText(2, 12, buf, 0xFFCCFF00);
        }
    }
    if (E.touchZones && ps && !ps->_gameOver) {
        const float W = (float)FlxG::width(), H = (float)FlxG::height();
        if (ps->ControlScheme() == 0) {
            // precision: anywhere charges; X alone sets the angle (135 deg at the left edge .. 405 deg at the right).
            for (int i = 0; i <= 6; i++) {
                float x = W * i / 6.0f;
                plat::strokeRect(x, H - 12, 1, 12, 0xC0FF0066);
            }
            plat::strokeRect(0, H - 40, W, 40, 0x80FF0066);
            plat::debugText(4, H - 38, "TOUCH X = AIM ANGLE", 0xFFFF0066);
        } else {
            // slingshot: a touch must start within 50 px of the centre; pull range = width * 0.33
            plat::strokeCircle(W / 2, H / 2, 50, 0xC0FF0066);
            plat::strokeCircle(W / 2, H - 160, W * 0.33f, 0x6000F1FE);
            plat::debugText(W / 2 - 44, H / 2 + 54, "START HERE", 0xFFFF0066);
        }
    }
}

// Exit codes (read by the launcher): 0 ok, 2 assets missing, 3 config error.
static bool assetsPresent(const std::string& dir) {
    return zig::fileExists(dir + "/ziglogo.png") && zig::fileExists(dir + "/Flixel.ttf");
}

int main(int argc, char** argv) {
    // --- zigcore: portable mode, data dir, logging, config (LAUNCHER_PLAN.md §3, §8) ---
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--portable")) zig::Paths::forcePortable(true);
    zig::Log::init("game");
    zig::Paths::migrateLegacy();
    std::string configFile;
    for (int i = 1; i + 1 < argc; i++)
        if (!strcmp(argv[i], "--config")) configFile = argv[i + 1];
    zig::Config& cfg = zig::Config::get();
    if (!configFile.empty()) cfg.load(configFile);

    FlxG::device = FlxDevice{};  // iPhone, non-retina, 320x480 portrait screen -> 480x320 landscape
    FlxG::device.assetDir = cfg.assetsDir();
    FlxG::device.windowScale = cfg.display.windowScale;
    float zoom = 1.0f;  // ZigguratIOS -init: zoom 1.0, modelZoom 1.0
    int shotFrame = -1;
    std::string shotFile;
    bool engineTest = false;
    std::string lang = cfg.game.language;
    std::string cliMode;  // --fullscreen / --windowed override display.mode
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--assets") && i + 1 < argc) FlxG::device.assetDir = argv[++i];
        else if (!strcmp(argv[i], "--shot") && i + 2 < argc) { shotFrame = atoi(argv[++i]); shotFile = argv[++i]; }
        else if (!strcmp(argv[i], "--zoom") && i + 1 < argc) zoom = (float)atof(argv[++i]);
        else if (zig::kTestHooks && !strcmp(argv[i], "--engine-test")) engineTest = true;  // dev only
        else if (!strcmp(argv[i], "--lang") && i + 1 < argc) lang = argv[++i];
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) {
            FlxG::device.windowScale = std::max(1, atoi(argv[++i]));
            cfg.display.customWidth = cfg.display.customHeight = 0;  // --scale wins over a custom size
        }
        else if (!strcmp(argv[i], "--fullscreen")) cliMode = "borderless";
        else if (!strcmp(argv[i], "--windowed")) cliMode = "windowed";
    }
    // Fallback asset locations for running without the launcher (dev builds).
    if (!assetsPresent(FlxG::device.assetDir)) {
        for (std::string alt : {zig::Paths::exeDir() + "assets", zig::Paths::exeDir() + "../assets", std::string("assets")})
            if (assetsPresent(alt)) { FlxG::device.assetDir = alt; break; }
    }
    if (!assetsPresent(FlxG::device.assetDir)) {
        SDL_Log("game: assets not found in '%s' - run the launcher to import your ZiGGURAT dump",
                FlxG::device.assetDir.c_str());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "ZiGGURAT",
                                 "Game files not found.\nStart ZigguratLauncher and select your ZiGGURAT dump.", nullptr);
        return 2;
    }
    // Language: "system" keeps the iOS-like behaviour (preferred locale); otherwise force it.
    if (!lang.empty() && lang != "system" && !SDL_getenv("ZIGGURAT_LANG")) SDL_setenv_unsafe("ZIGGURAT_LANG", lang.c_str(), 1);
    // Game options shown in the launcher mirror the original NSUserDefaults keys.
    {
        UserDefaults& d = UserDefaults::standard();
        d.set("preferredControlScheme", cfg.game.controlScheme == "slingshot" ? "slingshot" : "precision");
        d.set("aimGuide", cfg.game.aimGuide ? "on" : "off");
        if (cfg.game.showTutorial) d.remove("firstStartup");
        d.synchronize();
        if (cfg.game.showTutorial) { cfg.game.showTutorial = false; cfg.save(configFile); }
    }
    // Loose-file overrides (texture/sound packs) from <data>/overrides/ (LAUNCHER_PLAN.md §7.11).
    if (cfg.extras.overrides) {
        std::string od = zig::Paths::dataDir() + "overrides";
        zig::makeDirs(od);
        plat::setOverrideDir(od);
    }
    SDL_Log("game: assets=%s lang=%s scale=%d portable=%d", FlxG::device.assetDir.c_str(), lang.c_str(),
            FlxG::device.windowScale, (int)zig::Paths::portable());
    // Presentation and audio settings from the launcher config (LAUNCHER_PLAN.md §7.2 / §7.3).
    {
        plat::DisplaySettings ds;
        ds.mode = cliMode.empty() ? cfg.display.mode : cliMode;
        ds.monitor = cfg.display.monitor;
        if (cfg.display.customWidth > 0 && cfg.display.customHeight > 0) {
            ds.windowW = cfg.display.customWidth;
            ds.windowH = cfg.display.customHeight;
        }
        ds.fullscreenResolution = cfg.display.fullscreenResolution;
        ds.integerScaling = cfg.display.integerScaling;
        ds.filter = cfg.display.filter;
        ds.aspect = cfg.display.aspect;
        ds.borders = cfg.display.borders;
        ds.vsync = cfg.display.vsync;
        ds.showFps = cfg.display.showFps;
        plat::configureDisplay(ds);
        plat::AudioSettings as;
        as.master = cfg.audio.master;
        as.music = cfg.audio.music;
        as.sfx = cfg.audio.sfx;
        as.device = cfg.audio.device;
        as.muteInBackground = cfg.audio.muteInBackground;
        plat::configureAudio(as);
    }
    // Enhancements & accessibility (LAUNCHER_PLAN.md §7.9): defaults = original game.
    {
        enh() = cfg.enhancements;
        const auto& E = enh();
        FlxQuake::intensityScale = E.screenShake;
        if (E.fixedSeed != 0 && !SDL_getenv("ZIGGURAT_SEED")) {
            std::string seed = std::to_string((unsigned)E.fixedSeed);
            SDL_setenv_unsafe("ZIGGURAT_SEED", seed.c_str(), 1);
        }
        if (E.logLevel == "debug") SDL_SetLogPriorities(SDL_LOG_PRIORITY_DEBUG);
        else if (E.logLevel == "verbose") SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
        if (E.showOverlay || E.touchZones) plat::setDebugOverlayProvider(drawDebugOverlay);
        SDL_Log("game: enhancements preset=%s flash=%d shake=%.2f skipLogos=%d fixes=%d%d%d%d practice=%d",
                E.preset.c_str(), (int)E.reduceFlashing, E.screenShake, (int)E.skipLogos, (int)E.fixPurpleDoubleCount,
                (int)E.fixYellowAsRed, (int)E.fixRedPounce, (int)E.showJaOverlay, (int)E.practiceActive());
    }
    // Controllers / keyboard -> one virtual finger (gameplay) or a menu cursor (LAUNCHER_PLAN.md §7.4).
    // The context is read from the running state; the game logic itself is untouched.
    plat::configureInput(cfg.input);
    plat::setInputContextProvider([](int& scheme) {
        if (auto* ps = dynamic_cast<PlayState*>(FlxG::state())) {
            scheme = ps->ControlScheme();
            return ps->_gameOver ? 1 : 0;
        }
        scheme = 0;
        return 1;
    });
    // ZigguratIOS -init: [super initWithOrientation:1 state:@"StartState" zoom:1.0 ...]
    auto* game = engineTest
        ? (new FlxGame)->initWithOrientation(FlxGameOrientationLandscape, [] { return (new EngineTestState)->init(); }, zoom)
        : (new FlxGame)->initWithOrientation(FlxGameOrientationLandscape, [] { return makeStartState(); }, zoom);
    // ZigguratIOS -init: setFrameInterval:1 on retina devices (60 fps), engine default 2 (30 fps).
    // The launcher's "frame rate" setting picks between exactly these two original values.
    game->frameInterval = cfg.display.fps == 60 ? 1 : 2;
    game->shotFrame = shotFrame;
    game->shotFile = shotFile;
    for (int i = 1; i + 1 < argc; i++)
        if (!strcmp(argv[i], "--preview-seconds")) game->previewSeconds = atof(argv[i + 1]);
    int rc = game->run();
    // Write back options the player may have changed in the in-game Options menu.
    {
        UserDefaults& d = UserDefaults::standard();
        cfg.game.controlScheme = d.get("preferredControlScheme", "precision") == "slingshot" ? "slingshot" : "precision";
        cfg.game.aimGuide = d.get("aimGuide", "off") == "on";
        cfg.save(configFile);
    }
    return rc;
}
