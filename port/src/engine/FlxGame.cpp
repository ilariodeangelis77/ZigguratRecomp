//
//  FlxGame.cpp — FlxGame.m (+ the parts of FlxGLView.m that set up the drawable)
//
//  Copyright Semi Secret Software 2009-2010. All rights reserved.
//  C++ port: MIT license (see ENGINE_LICENSE.txt).
//
//  PORT: UIWindow/EAGL/CADisplayLink are replaced by an SDL window and a self-paced loop. The game is
//  rendered directly in landscape (no 90-degree GL rotation, no device autorotation animation, no
//  black "letterbox" vertices). Default.png splash and orientation notifications are dropped.
//
#include <SDL3/SDL.h>
#include "core/TestHooks.h"

#include "Flixel.h"

extern std::string flxPendingMusic;
std::vector<FlashFunction*>& flxDeferredCalls();

static unsigned int slowdown_ = 0;
static unsigned int slowdownCounter = 0;
static bool gameStarted = false;

void FlxGame::setSlowdown(unsigned int s) {
    slowdown_ = s;
    slowdownCounter = 0;
}
unsigned int FlxGame::slowdown() { return slowdown_; }

void FlxGame::setPaused(BOOL p) {
    _paused = p;
    if (_state) _state->setPaused(_paused);
}

// - (id) initWithOrientation:state:zoom:useTextureBufferZoom:modelZoom:
FlxGame* FlxGame::initWithOrientation(FlxGameOrientation GameOrientation, std::function<FlxState*()> InitialState, float Zoom,
                                      BOOL TextureBufferZoom, float ModelZoom) {
    FlxG::initialize();
    _zoom = Zoom;
    textureBufferZoom = TextureBufferZoom;
    modelZoom = ModelZoom;
    iPad = FlxG::iPad();
    frameInterval = FlxG::iPad() ? 1 : 2;
    autorotate = true;
    gameOrientation = GameOrientation;

    // FlxGLView: renderbuffer = UIScreen bounds * contentScaleFactor (portrait)
    const float boundsW = FlxG::device.pointsWidth, boundsH = FlxG::device.pointsHeight;
    const int scaleFactor = FlxG::retinaDisplay() ? 2 : 1;
    backingWidth = (int)boundsW * scaleFactor;
    backingHeight = (int)boundsH * scaleFactor;
    SDL_Log("backing dimensions: (%d,%d)", backingWidth, backingHeight);

    const bool portrait = gameOrientation == FlxGameOrientationPortrait;
    int gw, gh;
    if (textureBufferZoom) {
        if (FlxG::retinaDisplay()) {
            gw = (int)((portrait ? boundsW : boundsH) / _zoom);
            gh = (int)((portrait ? boundsH : boundsW) / _zoom);
        } else {
            gw = (int)((portrait ? boundsW : boundsH) / _zoom / 2);
            gh = (int)((portrait ? boundsH : boundsW) / _zoom / 2);
        }
    } else {
        if (FlxG::retinaDisplay()) {
            gw = (int)(portrait ? boundsW : boundsH);
            gh = (int)(portrait ? boundsH : boundsW);
        } else {
            gw = (int)((portrait ? boundsW : boundsH) / _zoom);
            gh = (int)((portrait ? boundsH : boundsW) / _zoom);
        }
    }
    // Render target in game orientation: the full renderbuffer, or the half-size FBO texture.
    int tw = portrait ? backingWidth : backingHeight;
    int th = portrait ? backingHeight : backingWidth;
    if (textureBufferZoom) {
        tw /= 2;
        th /= 2;
    }
    plat::init("ZiGGURAT", tw, th, FlxG::device.windowScale * (textureBufferZoom ? 2 : 1), FlxG::device.assetDir);
    plat::setProjectionZoom(textureBufferZoom ? 1.0f : _zoom);
    FlxG::setGameData(this, gw, gh, Zoom);
    _iState = InitialState;
    return this;
}

// - (void) start
void FlxGame::start() {
    _elapsed = 0;
    _total = 0;
    _state = nullptr;
    _gameXOffset = 0;
    _gameYOffset = 0;
    _paused = false;
    _created = false;
    if (_iState) {
        newState = _iState();
        newState->preProcess();
        plat::glClear();
        plat::present();
    }
}

// - (void) resetProjection;
void FlxGame::resetProjection() { plat::setProjectionZoom(textureBufferZoom ? 1.0f : _zoom); }

// - (void) switchState:(FlxState *)State;
void FlxGame::switchState(FlxState* State) {
    FlxG::unfollow();
    FlxG::flash()->stop();
    FlxG::fade()->stop();
    FlxG::quake()->stop();
    if (_state != nullptr) {
        _state->destroy();
        // [_state autorelease]: freed later by collectGraveyard(), never while its code may still run
        _graveyard.push_back({_state, _frameCount});
    }
    _state = State;
    gFlxZone = &_state->zone;
    _state->create();
}

// PORT: frees switched-out states and everything in their zones. A state is only freed once at
// least two frames have passed and no deferred FlashFunction call is pending (those may target
// objects of the old state, exactly like the retained targets of performSelector:afterDelay:).
std::vector<FlashFunction*>& flxDeferredCalls();
void FlxGame::collectGraveyard(bool force) {
    if (_graveyard.empty()) return;
    if (!force && !flxDeferredCalls().empty()) return;
    FlxZone* keep = gFlxZone;
    for (size_t i = 0; i < _graveyard.size();) {
        DeadState d = _graveyard[i];
        if (force || _frameCount - d.frame >= 2) {
            _graveyard.erase(_graveyard.begin() + i);
            if (gFlxZone == &d.state->zone) keep = _state ? &_state->zone : nullptr;
            d.state->zone.freeAll();
            delete d.state;
        } else {
            ++i;
        }
    }
    gFlxZone = keep;
}

// - (void) update
void FlxGame::update() {
    if (slowdown_ != 0) {
        slowdownCounter++;
        if (slowdownCounter < slowdown_) return;
        slowdownCounter = 0;
    }
    static double last = 0.0;
    double now = plat::nowSeconds();
    _elapsed = (now - last);
    last = now;
    // Test hook (port-only): ZIGGURAT_FIXED_DT=1 -> deterministic step of frameInterval/60 s, no frame pacing.
    static const bool fixedDt = zig::testEnv("ZIGGURAT_FIXED_DT") != nullptr;
    if (fixedDt) _elapsed = frameInterval / 60.0;
    if (!gameStarted) gameStarted = true;
    FlxG::setElapsed(_elapsed);
    if (_elapsed > FlxG::maxElapsed() && slowdown_ == 0) FlxG::setElapsed(FlxG::maxElapsed());
    FlxG::setElapsed(FlxG::elapsed() * FlxG::timeScale());

    if (_created) {
        FlxG::touches()->update();
        FlxG::updateSounds();
        if (_paused) {
        } else {
            FlxG::doFollow();
            _state->update();
            if (FlxG::flash()->exists) FlxG::flash()->update();
            if (FlxG::fade()->exists) FlxG::fade()->update();
            FlxG::quake()->update();
        }
        _state->preProcess();
        plat::glPushMatrix();
        plat::glClear();
        if (!textureBufferZoom) plat::glTranslatef(FlxG::quake()->x, FlxG::quake()->y);
        plat::glPushMatrix();
        _state->render();
        if (FlxG::flash()->exists) FlxG::flash()->render();
        if (FlxG::fade()->exists) FlxG::fade()->render();
        plat::glPopMatrix();
        plat::glPopMatrix();
        _state->postProcess();
    } else {
        _created = true;
        if (newState) {
            switchState(newState);
            newState->preProcess();
            newState = nullptr;
        }
        plat::glClear();
    }
    // [NSAutoreleasePool drain] at the end of the run-loop iteration: free switched-out states.
    _frameCount++;
    collectGraveyard();
    // PORT: with textureBufferZoom the original blitted the half-size texture (offset by quake*2);
    // here the integer-scaled presentation does the upscale and the quake offset is not re-applied.
}

// PORT: replaces CADisplayLink(frameInterval) + UIApplication run loop.
int FlxGame::run() {
    start();
    const double period = frameInterval / 60.0;
    double next = plat::nowSeconds();
    bool lastDown = false;
    const double runStart = plat::nowSeconds();
    for (int frame = 0;; frame++) {
        if (!plat::pumpEvents()) break;
        if (previewSeconds > 0 && plat::nowSeconds() - runStart >= previewSeconds) break;
        // UIKit touchesBegan/Moved/Ended -> -[FlxTouches processTouches:]
        const plat::Touch& t = plat::touch();
        const float toPoints = 1.0f / (FlxG::retinaDisplay() ? 2 : 1) * (textureBufferZoom ? 2 : 1);
        if (t.began || t.ended || t.moved || t.down != lastDown) {
            if (zig::testEnv("ZIGGURAT_TOUCHLOG")) SDL_Log("TOUCH frame=%d down=%d began=%d ended=%d moved=%d (%.0f,%.0f)", frame, t.down, t.began, t.ended, t.moved, t.x, t.y);
            FlxG::touches()->processTouches(t.down ? 1 : 0, CGPointMake(t.x * toPoints, t.y * toPoints));
        }
        lastDown = t.down;
        // performSelector:afterDelay:0 work queued during the previous frame
        if (!flxPendingMusic.empty()) {
            std::string m;
            m.swap(flxPendingMusic);
            FlxG::reallyPlayMusic(m);
        }
        if (!flxDeferredCalls().empty()) {
            auto calls = flxDeferredCalls();
            flxDeferredCalls().clear();
            for (auto* f : calls) f->execute();
        }
        update();
        plat::updateAudio(FlxG::elapsed());
        plat::drawOverlays();  // controller menu cursor
        if (frame == shotFrame) {
            plat::saveFrameBMP(shotFile.c_str());
            break;
        }
        // Test hook (port-only): ZIGGURAT_DUMP_EVERY=N saves every Nth frame to frames/frame_NNNNN.bmp,
        // ZIGGURAT_MAX_FRAMES=M quits after M frames. Mirrors touchHLE's TOUCHHLE_DUMP_EVERY.
        {
            static int dumpEvery = zig::testEnv("ZIGGURAT_DUMP_EVERY") ? atoi(zig::testEnv("ZIGGURAT_DUMP_EVERY")) : 0;
            static int maxFrames = zig::testEnv("ZIGGURAT_MAX_FRAMES") ? atoi(zig::testEnv("ZIGGURAT_MAX_FRAMES")) : 0;
            if (dumpEvery > 0 && frame % dumpEvery == 0) {
                SDL_CreateDirectory("frames");
                char name[64];
                snprintf(name, sizeof name, "frames/frame_%05d.bmp", frame);
                plat::saveFrameBMP(name);
            }
            if (maxFrames > 0 && frame >= maxFrames) break;
        }
        plat::present();
        next += period;
        double now = plat::nowSeconds();
        if (next < now - period) next = now;  // fell behind (window drag etc.)
        if (!zig::testEnv("ZIGGURAT_FIXED_DT")) plat::sleepUntil(next);
    }
    plat::shutdown();
    return 0;
}

// ---------------------------------------------------------------------------------------------
// Test-only heap hooks (determinism checks). Inert unless the env vars are set.
//   ZIGGURAT_HEAP_FILL=<byte>     fill every new allocation with this byte (detects uninitialised reads)
//   ZIGGURAT_HEAP_PERTURB=<bytes> allocate and hold a block at startup (shifts heap layout)
// ---------------------------------------------------------------------------------------------
#include <cstdlib>
#include <cstring>
#include <new>
namespace {
int heapFillByte() {
    static int v = [] {
        const char* e = zig::kTestHooks ? std::getenv("ZIGGURAT_HEAP_FILL") : nullptr;  // release: compiled out
        return e ? (int)(std::strtol(e, nullptr, 0) & 0xff) : -1;
    }();
    return v;
}
struct HeapPerturb {
    HeapPerturb() {
        if (const char* e = zig::kTestHooks ? std::getenv("ZIGGURAT_HEAP_PERTURB") : nullptr) {
            size_t n = (size_t)std::strtoul(e, nullptr, 0);
            if (n) { void* p = std::malloc(n); if (p) std::memset(p, 1, n); held = p; }
        }
    }
    void* held = nullptr;
} gHeapPerturb;
}  // namespace
void* operator new(size_t n) {
    void* p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    int f = heapFillByte();
    if (f >= 0) std::memset(p, f, n);
    return p;
}
void* operator new[](size_t n) { return ::operator new(n); }
// ZIGGURAT_HEAP_POISON=1 (test-only): freed blocks are filled with 0xDD and never reused, so any
// use-after-free reads garbage (and virtual calls crash) instead of silently working.
#ifdef _WIN32
#include <malloc.h>
static size_t blockSize(void* p) { return _msize(p); }
#else
#include <malloc.h>
static size_t blockSize(void* p) { return malloc_usable_size(p); }
#endif
static bool heapPoison() {
    static bool v = zig::kTestHooks && std::getenv("ZIGGURAT_HEAP_POISON") != nullptr;
    return v;
}
static void freeBlock(void* p) noexcept {
    if (!p) return;
    if (heapPoison()) {
        std::memset(p, 0xDD, blockSize(p));
        return;  // quarantine: leak on purpose so the poisoned block is never handed out again
    }
    std::free(p);
}
void operator delete(void* p) noexcept { freeBlock(p); }
void operator delete[](void* p) noexcept { freeBlock(p); }
void operator delete(void* p, size_t) noexcept { freeBlock(p); }
void operator delete[](void* p, size_t) noexcept { freeBlock(p); }
