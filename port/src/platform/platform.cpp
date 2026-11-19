#include "platform.h"
#include "core/TestHooks.h"

#include "core/AppIcon.h"
#include "core/FileIO.h"
#include "core/Paths.h"
#include "platform/pad.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

#include "stb_image.h"  // implementation: core/StbImage.cpp
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

namespace plat {
namespace {

SDL_Window* gWindow = nullptr;
SDL_Renderer* gRenderer = nullptr;
SDL_Texture* gTarget = nullptr;  // backing-resolution render target (the EAGL renderbuffer)
int gW = 480, gH = 320;
std::string gAssets;
std::unordered_map<std::string, Texture> gTextures;
Texture gSnapshot;
Touch gTouch;
SDL_BlendMode gScreenBlend;

// ---- GLES1 emulation state ----
struct Mat { float a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0; };  // x' = a x + c y + tx ; y' = b x + d y + ty
std::vector<Mat> gStack(1);
float gProjZoom = 1;
float gClear[4] = {0, 0, 0, 1};
Texture* gRenderTo = nullptr;
float gSavedZoom = 1;
std::vector<Mat> gSavedStack;

Mat mul(const Mat& m, const Mat& n) {  // m * n
    Mat r;
    r.a = m.a * n.a + m.c * n.b;
    r.b = m.b * n.a + m.d * n.b;
    r.c = m.a * n.c + m.c * n.d;
    r.d = m.b * n.c + m.d * n.d;
    r.tx = m.a * n.tx + m.c * n.ty + m.tx;
    r.ty = m.b * n.tx + m.d * n.ty + m.ty;
    return r;
}

// ---------------- audio ----------------
SDL_AudioDeviceID gDev = 0;
SDL_AudioSpec gDevSpec{};
struct Sound {
    SDL_AudioSpec spec{};
    std::vector<uint8_t> pcm;
    SDL_AudioStream* stream = nullptr;  // the "OpenAL source"
    bool looped = false;
};
std::unordered_map<std::string, std::unique_ptr<Sound>> gSounds;

struct Music {
    SDL_AudioStream* stream = nullptr;
    std::vector<int16_t> pcm;  // interleaved
    SDL_AudioSpec spec{};
    size_t cursor = 0;
    float volume = 1, fade = 1, fadeRate = 0;
    bool paused = false, playing = false;
    std::string name;
} gMusic;
float gMusicMaster = 1, gSfxMaster = 1;

Sound* getSound(const std::string& name) {
    auto it = gSounds.find(name);
    if (it != gSounds.end()) return it->second.get();
    if (!loadSound(name)) return nullptr;
    return gSounds[name].get();
}

// ---------------- presentation settings (launcher config) ----------------
DisplaySettings gDisp;
AudioSettings gAud;
bool gFocused = true;
SDL_FRect gView{0, 0, 480, 320};   // where the backing image lands, in render-output pixels
SDL_Texture* gSharp = nullptr;     // sharp_bilinear intermediate (backing * k)
int gSharpK = 0;
uint64_t gFpsT0 = 0;
int gFpsFrames = 0;
float gFpsValue = 0;
uint64_t gPresentCount = 0;

void applyDeviceGain() {
    if (gDev) SDL_SetAudioDeviceGain(gDev, gAud.master * ((gFocused || !gAud.muteInBackground) ? 1.0f : 0.0f));
}

SDL_DisplayID findDisplay(const std::string& name) {
    if (!name.empty()) {
        int n = 0;
        SDL_DisplayID* ids = SDL_GetDisplays(&n);
        SDL_DisplayID found = 0;
        for (int i = 0; ids && i < n; i++) {
            const char* dn = SDL_GetDisplayName(ids[i]);
            if (dn && name == dn) { found = ids[i]; break; }
        }
        SDL_free(ids);
        if (found) return found;
        SDL_Log("display: monitor '%s' not found, using primary", name.c_str());
    }
    return SDL_GetPrimaryDisplay();
}

// Fullscreen / windowed according to gDisp.mode.
void applyDisplayMode() {
    if (gDisp.mode == "borderless") {
        SDL_SetWindowFullscreenMode(gWindow, nullptr);  // desktop fullscreen
        SDL_SetWindowFullscreen(gWindow, true);
    } else if (gDisp.mode == "exclusive") {
        SDL_DisplayID d = SDL_GetDisplayForWindow(gWindow);
        SDL_DisplayMode m{};
        bool ok = false;
        int w = 0, h = 0;
        float hz = 0;
        if (gDisp.fullscreenResolution != "desktop" &&
            sscanf(gDisp.fullscreenResolution.c_str(), "%dx%d@%f", &w, &h, &hz) >= 2 && w > 0 && h > 0) {
            ok = SDL_GetClosestFullscreenDisplayMode(d, w, h, hz, false, &m);
            if (!ok) SDL_Log("display: no mode close to %s: %s", gDisp.fullscreenResolution.c_str(), SDL_GetError());
        }
        if (!ok) {
            const SDL_DisplayMode* dm = SDL_GetDesktopDisplayMode(d);
            if (dm) { m = *dm; ok = true; }
        }
        SDL_SetWindowFullscreenMode(gWindow, ok ? &m : nullptr);
        SDL_SetWindowFullscreen(gWindow, true);
        if (ok) SDL_Log("display: exclusive %dx%d@%.2f", m.w, m.h, m.refresh_rate);
    } else {
        SDL_SetWindowFullscreen(gWindow, false);
    }
    SDL_SyncWindow(gWindow);
}

// Destination rect of the backing image inside an output of ow x oh pixels.
SDL_FRect computeView(int ow, int oh) {
    SDL_FRect r{0, 0, (float)ow, (float)oh};
    if (gDisp.integerScaling) {
        int k = std::max(1, std::min(ow / gW, oh / gH));
        r.w = (float)(gW * k);
        r.h = (float)(gH * k);
    } else if (gDisp.aspect == "stretch") {
        return r;
    } else {
        float k = std::min((float)ow / gW, (float)oh / gH);
        r.w = std::floor(gW * k);
        r.h = std::floor(gH * k);
    }
    r.x = std::floor((ow - r.w) / 2);
    r.y = std::floor((oh - r.h) / 2);
    return r;
}

// Window coordinates (points) -> backing/game coordinates.
// Like a real touchscreen, the result is clamped to the screen (0..W-1, 0..H-1): a finger cannot touch
// outside the glass, but a mouse can be dragged into the letterbox borders or outside the window, which
// would feed the game positions the original never received (e.g. precision aim below 135 / above 405
// degrees). Returns false when the raw point was outside the game picture.
bool windowToGame(float wx, float wy, float& gx, float& gy) {
    int ww = 1, wh = 1, pw = 1, ph = 1;
    SDL_GetWindowSize(gWindow, &ww, &wh);
    SDL_GetWindowSizeInPixels(gWindow, &pw, &ph);
    SDL_FRect v = computeView(pw, ph);
    float px = wx * pw / std::max(1, ww), py = wy * ph / std::max(1, wh);
    gx = (px - v.x) * gW / std::max(1.0f, v.w);
    gy = (py - v.y) * gH / std::max(1.0f, v.h);
    bool inside = gx >= 0 && gy >= 0 && gx < (float)gW && gy < (float)gH;
    gx = std::min(std::max(gx, 0.0f), (float)(gW - 1));
    gy = std::min(std::max(gy, 0.0f), (float)(gH - 1));
    return inside;
}

}  // namespace

void configureDisplay(const DisplaySettings& s) { gDisp = s; }
void configureAudio(const AudioSettings& s) {
    gAud = s;
    gMusicMaster = s.music;
    gSfxMaster = s.sfx;
    applyDeviceGain();
}
void toggleFullscreen() {
    if (!gWindow) return;
    bool fs = (SDL_GetWindowFlags(gWindow) & SDL_WINDOW_FULLSCREEN) != 0;
    gDisp.mode = fs ? "windowed" : "borderless";
    applyDisplayMode();
}

// Loose-file overrides (texture/sound packs): <data>/overrides/<file> wins over the imported assets.
namespace {
std::string gOverrideDir;
std::unordered_map<std::string, std::string> gOverrideCache;  // file -> resolved path ("" = not overridden)
}  // namespace

void setOverrideDir(const std::string& dir) {
    gOverrideDir = dir;
    gOverrideCache.clear();
}

// UTF-8-safe loaders: read the file into memory via zig::readFile (wide APIs on Windows),
// then decode from memory. Paths with accents/CJK work regardless of the ANSI code page.
static unsigned char* loadImageU8(const std::string& path, int* w, int* h, int* n) {
    std::vector<uint8_t> bytes;
    if (!zig::readFile(path, bytes) || bytes.empty()) return nullptr;
    return stbi_load_from_memory(bytes.data(), (int)bytes.size(), w, h, n, 4);
}
static int16_t* loadWavU8(const std::string& path, unsigned int* ch, unsigned int* rate, drwav_uint64* frames) {
    std::vector<uint8_t> bytes;
    if (!zig::readFile(path, bytes) || bytes.empty()) return nullptr;
    return drwav_open_memory_and_read_pcm_frames_s16(bytes.data(), bytes.size(), ch, rate, frames, nullptr);
}
static int16_t* loadMp3U8(const std::string& path, drmp3_config* cfg, drmp3_uint64* frames) {
    std::vector<uint8_t> bytes;
    if (!zig::readFile(path, bytes) || bytes.empty()) return nullptr;
    return drmp3_open_memory_and_read_pcm_frames_s16(bytes.data(), bytes.size(), cfg, frames, nullptr);
}

std::string assetPath(const std::string& f) {
    if (!gOverrideDir.empty()) {
        auto it = gOverrideCache.find(f);
        if (it == gOverrideCache.end()) {
            std::string cand = gOverrideDir + "/" + f;
            bool hit = zig::fileExists(cand);
            if (hit) SDL_Log("overrides: using %s", cand.c_str());  // logged once per file
            it = gOverrideCache.emplace(f, hit ? cand : std::string()).first;
        }
        if (!it->second.empty()) return it->second;
    }
    return gAssets + "/" + f;
}

std::string prefPath(const std::string& f) {
    return zig::Paths::data(f);  // zigcore: pref dir or portable ./data
}

bool init(const char* title, int bw, int bh, int scale, const std::string& assetDir) {
    gW = bw; gH = bh; gAssets = assetDir;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD)) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return false;
    }
    // Window on the chosen monitor; windowed size from the launcher config (or backing * scale).
    SDL_DisplayID disp = findDisplay(gDisp.monitor);
    int ww = gDisp.windowW > 0 ? gDisp.windowW : bw * scale;
    int wh = gDisp.windowH > 0 ? gDisp.windowH : bh * scale;
    SDL_PropertiesID wp = SDL_CreateProperties();
    SDL_SetStringProperty(wp, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title);
    SDL_SetNumberProperty(wp, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, ww);
    SDL_SetNumberProperty(wp, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, wh);
    SDL_SetNumberProperty(wp, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED_DISPLAY(disp));
    SDL_SetNumberProperty(wp, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED_DISPLAY(disp));
    SDL_SetBooleanProperty(wp, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
    gWindow = SDL_CreateWindowWithProperties(wp);
    SDL_DestroyProperties(wp);
    zig::setWindowIcon(gWindow);
    if (!gWindow || !(gRenderer = SDL_CreateRenderer(gWindow, nullptr))) {
        SDL_Log("window: %s", SDL_GetError());
        return false;
    }
    applyDisplayMode();
    // FlxGame paces itself (CADisplayLink frameInterval); vsync is optional on top.
    SDL_SetRenderVSync(gRenderer, gDisp.vsync ? 1 : 0);
    // PORT: no SDL logical presentation; present() places the backing image itself (integer scale,
    // letterbox, stretch, filters, border art) and windowToGame() maps input back.
    SDL_Log("display: mode=%s %dx%d integer=%d filter=%s aspect=%s borders=%s vsync=%d", gDisp.mode.c_str(), ww, wh,
            (int)gDisp.integerScaling, gDisp.filter.c_str(), gDisp.aspect.c_str(), gDisp.borders.c_str(), (int)gDisp.vsync);
    gTarget = SDL_CreateTexture(gRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, bw, bh);
    SDL_SetTextureScaleMode(gTarget, SDL_SCALEMODE_NEAREST);
    SDL_SetRenderTarget(gRenderer, gTarget);
    gScreenBlend = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_COLOR, SDL_BLENDOPERATION_ADD,
                                              SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD);

    SDL_AudioSpec want{SDL_AUDIO_F32, 2, 48000};
    SDL_AudioDeviceID devId = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    if (!gAud.device.empty() && gAud.device != "default") {
        int n = 0;
        SDL_AudioDeviceID* ids = SDL_GetAudioPlaybackDevices(&n);
        for (int i = 0; ids && i < n; i++) {
            const char* dn = SDL_GetAudioDeviceName(ids[i]);
            if (dn && gAud.device == dn) { devId = ids[i]; break; }
        }
        if (devId == SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK) SDL_Log("audio: device '%s' not found, using default", gAud.device.c_str());
        SDL_free(ids);
    }
    gDev = SDL_OpenAudioDevice(devId, &want);
    if (!gDev) SDL_Log("audio: %s", SDL_GetError());
    else SDL_GetAudioDeviceFormat(gDev, &gDevSpec, nullptr);
    applyDeviceGain();
    return true;
}

int backingWidth() { return gW; }
int backingHeight() { return gH; }

void shutdown() {
    padShutdown();
    for (auto& [k, s] : gSounds) if (s && s->stream) SDL_DestroyAudioStream(s->stream);
    if (gMusic.stream) SDL_DestroyAudioStream(gMusic.stream);
    for (auto& [k, t] : gTextures) if (t.tex) SDL_DestroyTexture(t.tex);
    if (gDev) SDL_CloseAudioDevice(gDev);
    SDL_DestroyRenderer(gRenderer);
    SDL_DestroyWindow(gWindow);
    SDL_Quit();
}

double nowSeconds() { return SDL_GetTicksNS() / 1e9; }
void sleepUntil(double t) {
    double now = nowSeconds();
    if (t > now) SDL_DelayPrecise((Uint64)((t - now) * 1e9));
}

// Test hook: ZIGGURAT_TAPS="ms:x:y[:holdms];..." injects taps (game coordinates, like the mouse)
// `ms` after the first pumpEvents call. Mirrors touchHLE's TOUCHHLE_TAPS for side-by-side tests.
static void applyScriptedTaps() {
    struct Tap { uint64_t at, hold; float x, y; int stage; };
    static std::vector<Tap> taps;
    static uint64_t start = 0;
    static bool parsed = false;
    if (!parsed) {
        parsed = true;
        start = SDL_GetTicks();
        if (const char* spec = zig::testEnv("ZIGGURAT_TAPS")) {
            std::string s = spec;
            size_t pos = 0;
            while (pos < s.size()) {
                size_t end = s.find(';', pos);
                std::string item = s.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
                pos = end == std::string::npos ? s.size() : end + 1;
                Tap t{0, 120, 0, 0, 0};
                if (sscanf(item.c_str(), "%llu:%f:%f:%llu", (unsigned long long*)&t.at, &t.x, &t.y,
                           (unsigned long long*)&t.hold) >= 3)
                    taps.push_back(t);
            }
        }
    }
    uint64_t now = SDL_GetTicks() - start;
    // With ZIGGURAT_FIXED_DT the game runs unpaced: use a frame clock instead (one pumpEvents call =
    // ZIGGURAT_TAPS_FRAME_MS, default 1000/30 ms) so scripted runs are reproducible.
    if (zig::testEnv("ZIGGURAT_FIXED_DT")) {
        static uint64_t calls = 0;
        static const double frameMs = zig::testEnv("ZIGGURAT_TAPS_FRAME_MS") ? atof(zig::testEnv("ZIGGURAT_TAPS_FRAME_MS")) : 1000.0 / 30.0;
        now = (uint64_t)(calls++ * frameMs);
    }
    for (auto& t : taps) {
        if (t.stage == 0 && now >= t.at) {
            t.stage = 1;
            gTouch.x = t.x; gTouch.y = t.y; gTouch.down = true; gTouch.began = true;
            return;  // one transition per frame so began/ended are seen separately
        }
        if (t.stage == 1 && now >= t.at + t.hold) {
            t.stage = 2;
            gTouch.x = t.x; gTouch.y = t.y; gTouch.down = false; gTouch.ended = true;
            return;
        }
    }
}

// True when a test hook (ZIGGURAT_TAPS / ZIGGURAT_PAD) scripts the input: real pointer events are ignored.
static bool scriptedInputActive() {
    static const bool on = zig::testEnv("ZIGGURAT_TAPS") != nullptr || zig::testEnv("ZIGGURAT_PAD") != nullptr;
    return on;
}

bool pumpEvents() {
    gTouch.began = gTouch.ended = gTouch.moved = false;
    applyScriptedTaps();
    // Test hook (port-only): ZIGGURAT_SYNTH_CLICK="frame:wx:wy[:dragx:dragy]" pushes a mouse down at
    // WINDOW coordinates into this app's own event queue (no OS input), optionally drags to a second
    // point (which may lie outside the window, as with mouse capture) and releases there.
    if (const char* sc = zig::testEnv("ZIGGURAT_SYNTH_CLICK")) {
        static long n = 0;
        static long at = -1;
        static float sx = 0, sy = 0, ex = 0, ey = 0;
        static bool drag = false;
        if (at < 0) {
            int k = sscanf(sc, "%ld:%f:%f:%f:%f", &at, &sx, &sy, &ex, &ey);
            if (k == 5) drag = true;
            else if (k != 3) at = 0x7fffffff;
        }
        n++;
        if (n == at || n == at + 3 || (drag && n == at + 2)) {
            SDL_Event ev{};
            if (drag && n == at + 2) {
                ev.type = SDL_EVENT_MOUSE_MOTION;
                ev.motion.windowID = SDL_GetWindowID(gWindow);
                ev.motion.state = SDL_BUTTON_LMASK;
                ev.motion.x = ex;
                ev.motion.y = ey;
            } else {
                ev.type = n == at ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
                ev.button.windowID = SDL_GetWindowID(gWindow);
                ev.button.button = SDL_BUTTON_LEFT;
                ev.button.down = n == at;
                ev.button.x = (drag && n != at) ? ex : sx;
                ev.button.y = (drag && n != at) ? ey : sy;
            }
            SDL_PushEvent(&ev);
        }
    }
    SDL_Event e;
    bool pointerUsed = false;  // mouse button / drag or real touch this frame (wins over the pad)
    static bool padReady = false;
    if (!padReady) { padReady = true; padInit(); }
    while (SDL_PollEvent(&e)) {
        padHandleEvent(e);
        switch (e.type) {
            case SDL_EVENT_QUIT: return false;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
            case SDL_EVENT_MOUSE_MOTION: {
                if (!padMouseAsTouch()) break;  // launcher: MOUSE AS FINGER = OFF
                // Test determinism: while a scripted-input hook drives the finger, real OS pointer
                // events (e.g. the desktop cursor resting over the new window) must not move it.
                if (scriptedInputActive() && !zig::testEnv("ZIGGURAT_SYNTH_CLICK")) break;
                if (e.type == SDL_EVENT_MOUSE_MOTION && e.motion.which == SDL_TOUCH_MOUSEID) break;
                if (e.type != SDL_EVENT_MOUSE_MOTION && e.button.which == SDL_TOUCH_MOUSEID) break;
                float x, y;
                bool inside = windowToGame(e.type == SDL_EVENT_MOUSE_MOTION ? e.motion.x : e.button.x,
                                           e.type == SDL_EVENT_MOUSE_MOTION ? e.motion.y : e.button.y, x, y);
                if (e.type == SDL_EVENT_MOUSE_MOTION && !gTouch.down) break;  // hover is not a touch
                // a press outside the game picture is like touching the phone's bezel: no touch
                if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !inside) break;
                if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && !gTouch.down) break;
                pointerUsed = true;
                gTouch.x = x; gTouch.y = y;
                if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) { gTouch.down = true; gTouch.began = true; }
                else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) { gTouch.down = false; gTouch.ended = true; }
                else gTouch.moved = true;
                break;
            }
            case SDL_EVENT_FINGER_DOWN:
            case SDL_EVENT_FINGER_UP:
            case SDL_EVENT_FINGER_MOTION: {
                if (scriptedInputActive()) break;  // see the mouse case above
                int ww, wh;
                SDL_GetWindowSize(gWindow, &ww, &wh);
                float lx, ly;
                bool inside = windowToGame(e.tfinger.x * ww, e.tfinger.y * wh, lx, ly);
                if (e.type == SDL_EVENT_FINGER_DOWN && !inside) break;   // touch in the letterbox border
                if (e.type != SDL_EVENT_FINGER_DOWN && !gTouch.down) break;
                pointerUsed = true;
                gTouch.x = lx; gTouch.y = ly;
                if (e.type == SDL_EVENT_FINGER_DOWN) { gTouch.down = true; gTouch.began = true; }
                else if (e.type == SDL_EVENT_FINGER_UP) { gTouch.down = false; gTouch.ended = true; }
                else gTouch.moved = true;
                break;
            }
            case SDL_EVENT_KEY_DOWN:
                if (e.key.key == SDLK_F11 && !e.key.repeat) toggleFullscreen();
                break;
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                gFocused = e.type == SDL_EVENT_WINDOW_FOCUS_GAINED;
                applyDeviceGain();  // mute in background
                break;
            default: break;
        }
    }
    padUpdate(gTouch, pointerUsed, gW, gH);  // controller / keyboard -> virtual finger
    return true;
}

const Touch& touch() { return gTouch; }

// ---------------- simple sprite API ----------------
const Texture* texture(const std::string& name) {
    auto it = gTextures.find(name);
    if (it != gTextures.end()) return it->second.tex ? &it->second : nullptr;
    Texture t;
    int w, h, n;
    unsigned char* px = loadImageU8(assetPath(name), &w, &h, &n);
    if (!px) {
        SDL_Log("couldn't find texture %s", name.c_str());
        gTextures[name] = t;
        return nullptr;
    }
    SDL_Surface* s = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ABGR8888, px, w * 4);
    t.tex = SDL_CreateTextureFromSurface(gRenderer, s);
    SDL_SetTextureScaleMode(t.tex, SDL_SCALEMODE_NEAREST);
    t.w = t.paddedW = w; t.h = t.paddedH = h;
    SDL_DestroySurface(s);
    stbi_image_free(px);
    return &(gTextures[name] = t);
}

void clear(float r, float g, float b) {
    SDL_SetRenderTarget(gRenderer, gTarget);
    SDL_SetRenderDrawColorFloat(gRenderer, r, g, b, 1);
    SDL_RenderClear(gRenderer);
}

void draw(const Texture* t, float sx, float sy, float sw, float sh, float x, float y, float w, float h,
          float angle, float ox, float oy, uint32_t argb, Blend blend, bool flipX) {
    if (!t) return;
    SDL_SetTextureColorMod(t->tex, (argb >> 16) & 255, (argb >> 8) & 255, argb & 255);
    SDL_SetTextureAlphaMod(t->tex, argb >> 24);
    SDL_SetTextureBlendMode(t->tex, blend == Blend::Additive ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
    SDL_FRect src{sx, sy, sw, sh}, dst{x, y, w, h};
    SDL_FPoint c{ox, oy};
    SDL_RenderTextureRotated(gRenderer, t->tex, &src, &dst, angle, &c, flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

void fillRect(float x, float y, float w, float h, uint32_t argb) {
    SDL_SetRenderDrawBlendMode(gRenderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(gRenderer, (argb >> 16) & 255, (argb >> 8) & 255, argb & 255, argb >> 24);
    SDL_FRect r{x, y, w, h};
    SDL_RenderFillRect(gRenderer, &r);
}

// Enhancements debug overlay (provider set by the game; nullptr = off).
static void (*gDebugOverlay)() = nullptr;
void setDebugOverlayProvider(void (*fn)()) { gDebugOverlay = fn; }
void debugText(float x, float y, const char* text, uint32_t argb) {
    SDL_SetRenderTarget(gRenderer, gTarget);
    SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 160);
    SDL_FRect bg{x - 1, y - 1, (float)(8 * SDL_strlen(text) + 2), 10};
    SDL_SetRenderDrawBlendMode(gRenderer, SDL_BLENDMODE_BLEND);
    SDL_RenderFillRect(gRenderer, &bg);
    SDL_SetRenderDrawColor(gRenderer, (argb >> 16) & 255, (argb >> 8) & 255, argb & 255, argb >> 24);
    SDL_RenderDebugText(gRenderer, x, y, text);
}
void strokeRect(float x, float y, float w, float h, uint32_t argb) {
    SDL_SetRenderTarget(gRenderer, gTarget);
    SDL_SetRenderDrawBlendMode(gRenderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(gRenderer, (argb >> 16) & 255, (argb >> 8) & 255, argb & 255, argb >> 24);
    SDL_FRect r{x, y, w, h};
    SDL_RenderRect(gRenderer, &r);
}
void strokeCircle(float cx, float cy, float r, uint32_t argb) {
    SDL_SetRenderTarget(gRenderer, gTarget);
    SDL_SetRenderDrawBlendMode(gRenderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(gRenderer, (argb >> 16) & 255, (argb >> 8) & 255, argb & 255, argb >> 24);
    const int n = 48;
    SDL_FPoint pts[n + 1];
    for (int i = 0; i <= n; i++) {
        float a = (float)i / n * 6.2831853f;
        pts[i] = {cx + r * std::cos(a), cy + r * std::sin(a)};
    }
    SDL_RenderLines(gRenderer, pts, n + 1);
}

// Presentation of the backing image (the EAGL renderbuffer) into the window, per launcher settings.
// Overlays drawn into the game image (before frame dumps / presentation).
void drawOverlays() {
    if (gDebugOverlay) gDebugOverlay();
    // Controller menu cursor: the game's own reticle sprite (frame 0 of img_Reticle.png, 15x15).
    {
        float cx, cy;
        if (padCursor(cx, cy)) {
            if (const Texture* r = texture("img_Reticle.png")) {
                SDL_SetRenderTarget(gRenderer, gTarget);
                SDL_SetTextureColorMod(r->tex, 255, 255, 255);
                SDL_SetTextureAlphaMod(r->tex, 255);
                SDL_SetTextureBlendMode(r->tex, SDL_BLENDMODE_BLEND);
                SDL_FRect src{0, 0, 15, 15}, dst{std::floor(cx) - 7, std::floor(cy) - 7, 15, 15};
                SDL_RenderTexture(gRenderer, r->tex, &src, &dst);
            }
        }
    }
}

void present() {
    SDL_SetRenderTarget(gRenderer, nullptr);
    int ow = 1, oh = 1;
    SDL_GetCurrentRenderOutputSize(gRenderer, &ow, &oh);
    SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);
    SDL_RenderClear(gRenderer);
    gView = computeView(ow, oh);

    // Border art behind the letterboxed image (aspect-fill "cover" scaling).
    if (gDisp.borders != "black" && (gView.w < ow || gView.h < oh)) {
        const char* name = gDisp.borders == "sunset" ? "sky_sunset.png" : "sky_night.png";
        if (const Texture* sky = texture(name)) {
            float k = std::max((float)ow / sky->w, (float)oh / sky->h);
            SDL_FRect d{(ow - sky->w * k) / 2, (oh - sky->h * k) / 2, sky->w * k, sky->h * k};
            // "blur" falls back to a darkened night sky (no blur shader on SDL_Renderer).
            Uint8 dim = gDisp.borders == "blur" ? 0x50 : 0xB0;
            SDL_SetTextureColorMod(sky->tex, dim, dim, dim);
            SDL_SetTextureAlphaMod(sky->tex, 255);
            SDL_SetTextureBlendMode(sky->tex, SDL_BLENDMODE_NONE);
            SDL_RenderTexture(gRenderer, sky->tex, nullptr, &d);
            SDL_SetTextureColorMod(sky->tex, 255, 255, 255);
        }
    }

    SDL_SetTextureBlendMode(gTarget, SDL_BLENDMODE_NONE);
    if (gDisp.integerScaling || gDisp.filter == "nearest") {
        SDL_SetTextureScaleMode(gTarget, SDL_SCALEMODE_NEAREST);
        SDL_RenderTexture(gRenderer, gTarget, nullptr, &gView);
    } else if (gDisp.filter == "sharp_bilinear") {
        // Integer nearest prescale into an intermediate, then linear to the final (non-integer) size.
        int k = std::max(1, (int)std::floor(std::min(gView.w / gW, gView.h / gH)));
        if (k != gSharpK || !gSharp) {
            if (gSharp) SDL_DestroyTexture(gSharp);
            gSharp = SDL_CreateTexture(gRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, gW * k, gH * k);
            SDL_SetTextureScaleMode(gSharp, SDL_SCALEMODE_LINEAR);
            SDL_SetTextureBlendMode(gSharp, SDL_BLENDMODE_NONE);
            gSharpK = k;
        }
        SDL_SetRenderTarget(gRenderer, gSharp);
        SDL_SetTextureScaleMode(gTarget, SDL_SCALEMODE_NEAREST);
        SDL_RenderTexture(gRenderer, gTarget, nullptr, nullptr);
        SDL_SetRenderTarget(gRenderer, nullptr);
        SDL_RenderTexture(gRenderer, gSharp, nullptr, &gView);
    } else {  // linear
        SDL_SetTextureScaleMode(gTarget, SDL_SCALEMODE_LINEAR);
        SDL_RenderTexture(gRenderer, gTarget, nullptr, &gView);
        SDL_SetTextureScaleMode(gTarget, SDL_SCALEMODE_NEAREST);
    }

    // FPS counter (presented frames per second).
    uint64_t now = SDL_GetTicks();
    if (!gFpsT0) gFpsT0 = now;
    gFpsFrames++;
    if (now - gFpsT0 >= 1000) {
        gFpsValue = gFpsFrames * 1000.0f / (float)(now - gFpsT0);
        gFpsFrames = 0;
        gFpsT0 = now;
        if (zig::testEnv("ZIGGURAT_FPSLOG")) SDL_Log("fps: %.2f", gFpsValue);
    }
    if (gDisp.showFps) {
        char buf[32];
        snprintf(buf, sizeof buf, "%.1f FPS", gFpsValue);
        float s = std::max(1.0f, std::floor(oh / 360.0f));
        SDL_SetRenderScale(gRenderer, s, s);
        SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 160);
        SDL_SetRenderDrawBlendMode(gRenderer, SDL_BLENDMODE_BLEND);
        SDL_FRect bg{2, 2, (float)(strlen(buf) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE + 4), SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE + 4.0f};
        SDL_RenderFillRect(gRenderer, &bg);
        SDL_SetRenderDrawColor(gRenderer, 255, 255, 255, 255);
        SDL_RenderDebugText(gRenderer, 4, 4, buf);
        SDL_SetRenderScale(gRenderer, 1, 1);
    }

    // Test hook (port-only): ZIGGURAT_WINDOW_SHOT=<file.bmp> saves the final composited window frame
    // (after scaling/borders) at present #ZIGGURAT_WINDOW_SHOT_FRAME (default 60).
    gPresentCount++;
    if (const char* shot = zig::testEnv("ZIGGURAT_WINDOW_SHOT")) {
        static const uint64_t at = zig::testEnv("ZIGGURAT_WINDOW_SHOT_FRAME") ? strtoull(zig::testEnv("ZIGGURAT_WINDOW_SHOT_FRAME"), nullptr, 10) : 60;
        if (gPresentCount == at) {
            if (SDL_Surface* s = SDL_RenderReadPixels(gRenderer, nullptr)) {
                SDL_SaveBMP(s, shot);
                SDL_Log("window shot %dx%d -> %s (view %.0f,%.0f %.0fx%.0f)", s->w, s->h, shot, gView.x, gView.y, gView.w, gView.h);
                SDL_DestroySurface(s);
            }
        }
    }

    SDL_RenderPresent(gRenderer);
    SDL_SetRenderTarget(gRenderer, gTarget);
}

const Texture* snapshot() {
    SDL_Surface* s = SDL_RenderReadPixels(gRenderer, nullptr);
    if (!s) return nullptr;
    if (gSnapshot.tex) SDL_DestroyTexture(gSnapshot.tex);
    gSnapshot.tex = SDL_CreateTextureFromSurface(gRenderer, s);
    SDL_SetTextureScaleMode(gSnapshot.tex, SDL_SCALEMODE_NEAREST);
    gSnapshot.w = gSnapshot.paddedW = s->w; gSnapshot.h = gSnapshot.paddedH = s->h;
    SDL_DestroySurface(s);
    return &gSnapshot;
}

bool saveFrameBMP(const char* file) {
    SDL_SetRenderTarget(gRenderer, gTarget);
    SDL_Surface* s = SDL_RenderReadPixels(gRenderer, nullptr);
    if (!s) return false;
    bool ok = SDL_SaveBMP(s, file);
    SDL_DestroySurface(s);
    return ok;
}

// ---------------- GLES1 emulation ----------------
bool loadImageRGBA(const std::string& name, std::vector<uint8_t>& rgba, int& w, int& h) {
    int n;
    unsigned char* px = loadImageU8(assetPath(name), &w, &h, &n);
    if (!px) return false;
    rgba.assign(px, px + (size_t)w * h * 4);
    stbi_image_free(px);
    return true;
}

Texture* createTexture(const uint8_t* rgba, int w, int h, int pw, int ph, bool linear) {
    auto* t = new Texture;
    t->w = w; t->h = h; t->paddedW = pw; t->paddedH = ph;
    t->tex = SDL_CreateTexture(gRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, pw, ph);
    SDL_UpdateTexture(t->tex, nullptr, rgba, pw * 4);
    SDL_SetTextureScaleMode(t->tex, linear ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
    return t;
}

void destroyTexture(Texture* t) {
    if (!t) return;
    if (t->tex) SDL_DestroyTexture(t->tex);
    delete t;
}

void glPushMatrix() { gStack.push_back(gStack.back()); }
void glPopMatrix() { if (gStack.size() > 1) gStack.pop_back(); }
void glLoadIdentity() { gStack.back() = Mat{}; }
void glTranslatef(float x, float y) { Mat t; t.tx = x; t.ty = y; gStack.back() = mul(gStack.back(), t); }
void glScalef(float x, float y) { Mat s; s.a = x; s.d = y; gStack.back() = mul(gStack.back(), s); }
void glRotatef(float deg) {
    float r = deg * (float)M_PI / 180.0f, c = cosf(r), s = sinf(r);
    Mat m; m.a = c; m.b = s; m.c = -s; m.d = c;
    gStack.back() = mul(gStack.back(), m);
}
void setProjectionZoom(float z) { gProjZoom = z; }
void setClearColor(float r, float g, float b, float a) { gClear[0] = r; gClear[1] = g; gClear[2] = b; gClear[3] = a; }
void glClear() {
    SDL_SetRenderDrawColorFloat(gRenderer, gClear[0], gClear[1], gClear[2], 1.0f);
    SDL_RenderClear(gRenderer);
}

void drawStrip(const Texture* t, int n, const float* xy, const float* uv, const uint8_t* rgba, Blend blend) {
    if (n < 3) return;
    const Mat& m = gStack.back();
    const float z = gProjZoom;
    std::vector<SDL_Vertex> v(n);
    for (int i = 0; i < n; i++) {
        float x = xy[i * 2], y = xy[i * 2 + 1];
        v[i].position.x = z * (m.a * x + m.c * y + m.tx);
        v[i].position.y = z * (m.b * x + m.d * y + m.ty);
        if (rgba) v[i].color = {rgba[i * 4] / 255.f, rgba[i * 4 + 1] / 255.f, rgba[i * 4 + 2] / 255.f, rgba[i * 4 + 3] / 255.f};
        else v[i].color = {1, 1, 1, 1};
        v[i].tex_coord = uv ? SDL_FPoint{uv[i * 2], uv[i * 2 + 1]} : SDL_FPoint{0, 0};
    }
    std::vector<int> idx;
    idx.reserve((n - 2) * 3);
    for (int i = 0; i + 2 < n; i++) {  // triangle strip -> triangle list (GL winding irrelevant, no culling)
        idx.push_back(i); idx.push_back(i + 1); idx.push_back(i + 2);
    }
    SDL_BlendMode bm = blend == Blend::None ? SDL_BLENDMODE_NONE
                       : blend == Blend::Screen ? gScreenBlend
                       : blend == Blend::Additive ? SDL_BLENDMODE_ADD_PREMULTIPLIED
                                                  : SDL_BLENDMODE_BLEND_PREMULTIPLIED;
    if (t && t->tex) SDL_SetTextureBlendMode(t->tex, bm);
    else SDL_SetRenderDrawBlendMode(gRenderer, bm);
    SDL_RenderGeometry(gRenderer, t ? t->tex : nullptr, v.data(), n, idx.data(), (int)idx.size());
}

Texture* createRenderTarget(int w, int h) {
    auto* t = new Texture;
    t->w = t->paddedW = w; t->h = t->paddedH = h;
    t->tex = SDL_CreateTexture(gRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, w, h);
    SDL_SetTextureScaleMode(t->tex, SDL_SCALEMODE_NEAREST);
    SDL_SetRenderTarget(gRenderer, t->tex);
    SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 0);
    SDL_RenderClear(gRenderer);
    SDL_SetRenderTarget(gRenderer, gRenderTo ? gRenderTo->tex : gTarget);
    return t;
}
void beginRenderTo(Texture* t) {
    gRenderTo = t;
    gSavedZoom = gProjZoom;
    gSavedStack = gStack;
    gProjZoom = 1;
    gStack.assign(1, Mat{});
    SDL_SetRenderTarget(gRenderer, t->tex);
}
void endRenderTo() {
    gRenderTo = nullptr;
    gProjZoom = gSavedZoom;
    gStack = gSavedStack;
    SDL_SetRenderTarget(gRenderer, gTarget);
}

// ---------------- audio ----------------
bool loadSound(const std::string& name) {
    if (gSounds.count(name)) return gSounds[name] != nullptr;
    unsigned int ch, rate;
    drwav_uint64 frames;
    int16_t* data = loadWavU8(assetPath(name), &ch, &rate, &frames);
    if (!data) {
        gSounds[name] = nullptr;
        return false;
    }
    auto s = std::make_unique<Sound>();
    s->spec = {SDL_AUDIO_S16, (int)ch, (int)rate};
    s->pcm.assign((uint8_t*)data, (uint8_t*)data + frames * ch * 2);
    drwav_free(data, nullptr);
    if (gDev) {
        s->stream = SDL_CreateAudioStream(&s->spec, &gDevSpec);
        SDL_BindAudioStream(gDev, s->stream);
    }
    gSounds[name] = std::move(s);
    return true;
}

void playSound(const std::string& name, float gain, bool looped) {
    padRumbleForSound(name);  // optional rumble enhancement (off by default)
    Sound* s = getSound(name);
    if (!s || !s->stream) return;
    // alSourcePlay on a playing source restarts it from the beginning.
    SDL_ClearAudioStream(s->stream);
    SDL_SetAudioStreamGain(s->stream, gain * gSfxMaster);  // game volume * launcher SFX volume
    SDL_PutAudioStreamData(s->stream, s->pcm.data(), (int)s->pcm.size());
    s->looped = looped;
}

void stopSound(const std::string& name) {
    auto it = gSounds.find(name);
    if (it == gSounds.end() || !it->second || !it->second->stream) return;
    SDL_ClearAudioStream(it->second->stream);
    it->second->looped = false;
}

bool soundPlaying(const std::string& name) {
    auto it = gSounds.find(name);
    if (it == gSounds.end() || !it->second || !it->second->stream) return false;
    return SDL_GetAudioStreamQueued(it->second->stream) > 0;
}

bool playMusic(const std::string& name, float volume) {
    if (!gDev) return false;
    if (gMusic.name != name) {
        drmp3_config cfg;
        drmp3_uint64 frames;
        int16_t* d = loadMp3U8(assetPath(name), &cfg, &frames);
        if (!d) { SDL_Log("unable to load music %s", name.c_str()); return false; }
        gMusic.pcm.assign(d, d + frames * cfg.channels);
        drmp3_free(d, nullptr);
        gMusic.spec = {SDL_AUDIO_S16, (int)cfg.channels, (int)cfg.sampleRate};
        gMusic.name = name;
        if (gMusic.stream) SDL_DestroyAudioStream(gMusic.stream);
        gMusic.stream = SDL_CreateAudioStream(&gMusic.spec, &gDevSpec);
        SDL_BindAudioStream(gDev, gMusic.stream);
    }
    SDL_ClearAudioStream(gMusic.stream);
    SDL_ResumeAudioStreamDevice(gMusic.stream);
    gMusic.cursor = 0; gMusic.volume = volume; gMusic.fade = 1; gMusic.fadeRate = 0;
    gMusic.playing = true; gMusic.paused = false;
    return true;
}
void setMusicVolume(float v) { gMusic.volume = v; }
void stopMusic() {
    gMusic.playing = false;
    if (gMusic.stream) SDL_ClearAudioStream(gMusic.stream);
}
void pauseMusic() { gMusic.paused = true; }
void unpauseMusic() { gMusic.paused = false; }
bool musicPlaying() { return gMusic.playing && !gMusic.paused; }
void fadeOutMusic(float seconds) { gMusic.fadeRate = seconds > 0 ? 1.0f / seconds : 1000.0f; }
void setMasterVolumes(float m, float s) { gMusicMaster = m; gSfxMaster = s; }

void updateAudio(float elapsed) {
    for (auto& [k, s] : gSounds)
        if (s && s->stream && s->looped && SDL_GetAudioStreamQueued(s->stream) < (int)s->pcm.size() / 2)
            SDL_PutAudioStreamData(s->stream, s->pcm.data(), (int)s->pcm.size());
    if (!gMusic.stream) return;
    if (!gMusic.playing || gMusic.paused) {
        SDL_SetAudioStreamGain(gMusic.stream, 0);
        if (!gMusic.playing) SDL_ClearAudioStream(gMusic.stream);
        return;
    }
    if (gMusic.fadeRate > 0) {
        gMusic.fade -= gMusic.fadeRate * elapsed;
        if (gMusic.fade <= 0) { stopMusic(); return; }
    }
    SDL_SetAudioStreamGain(gMusic.stream, gMusic.volume * gMusic.fade * gMusicMaster);
    const int bytesPerFrame = gMusic.spec.channels * 2;
    const int target = gMusic.spec.freq / 4 * bytesPerFrame;  // ~250 ms queued
    while (SDL_GetAudioStreamQueued(gMusic.stream) < target) {
        size_t total = gMusic.pcm.size();
        size_t n = std::min<size_t>(total - gMusic.cursor, gMusic.spec.freq / 20 * gMusic.spec.channels);
        SDL_PutAudioStreamData(gMusic.stream, gMusic.pcm.data() + gMusic.cursor, (int)(n * 2));
        gMusic.cursor += n;
        if (gMusic.cursor >= total) gMusic.cursor = 0;  // numberOfLoops = -1
    }
}

}  // namespace plat
