// Platform layer for the ZiGGURAT port: replaces EAGL/GLES1 (FlxGLView/FlxGame),
// OpenAL + AVAudioPlayer (FlxG audio), UITouch (FlxTouches) and NSUserDefaults.
#pragma once
#include <SDL3/SDL.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace plat {

struct Texture {
    SDL_Texture* tex = nullptr;
    int w = 0, h = 0;              // image size
    int paddedW = 0, paddedH = 0;  // power-of-two backing size (SemiSecretTexture paddedSize)
};

struct Touch {
    bool down = false, began = false, ended = false, moved = false;
    float x = 0, y = 0;  // backing-pixel coordinates (== UIKit points on a non-retina device)
};

enum class Blend { Normal, Additive, Screen, None };

// ---------------- presentation / audio settings (launcher config, LAUNCHER_PLAN.md §7.2/§7.3) ----------------
// Call configureDisplay()/configureAudio() before init(); defaults reproduce the previous behaviour.
struct DisplaySettings {
    std::string mode = "windowed";        // windowed | borderless | exclusive
    std::string monitor;                  // display name, empty = primary
    int windowW = 0, windowH = 0;         // windowed size in pixels; 0 = backing size * windowScale
    std::string fullscreenResolution = "desktop";  // exclusive: "desktop" or "WxH" / "WxH@Hz"
    bool integerScaling = true;
    std::string filter = "nearest";       // nearest | linear | sharp_bilinear (ignored when integerScaling)
    std::string aspect = "original";      // original (3:2 letterbox) | stretch
    std::string borders = "black";        // black | night_sky | sunset | blur
    bool vsync = false;
    bool showFps = false;
};
struct AudioSettings {
    float master = 1, music = 1, sfx = 1;  // multiply on top of the game's own MusicVolume/SoundEffectsVolume
    std::string device = "default";
    bool muteInBackground = false;
};
void configureDisplay(const DisplaySettings& s);
void configureAudio(const AudioSettings& s);
// Runtime toggle (also bound to F11): windowed <-> borderless fullscreen.
void toggleFullscreen();

// backingW/H: the render target size (what EAGL's renderbuffer was).
bool init(const char* title, int backingW, int backingH, int windowScale, const std::string& assetDir);
void shutdown();
int backingWidth();
int backingHeight();

// Returns false when the app should quit. Updates input.
bool pumpEvents();
double nowSeconds();
void sleepUntil(double t);

// ---------------- simple sprite API (smoke tests) ----------------
const Texture* texture(const std::string& name);  // cached, straight alpha, loads assets/<name>
void clear(float r, float g, float b);
void draw(const Texture* t, float sx, float sy, float sw, float sh, float x, float y, float w, float h,
          float angleDeg = 0, float ox = 0, float oy = 0, uint32_t argb = 0xFFFFFFFF, Blend blend = Blend::Normal,
          bool flipX = false);
void fillRect(float x, float y, float w, float h, uint32_t argb);
void drawOverlays();  // controller menu cursor etc. (call before present / frame dumps)
// Enhancements: debug overlay drawn into the game image by game code (touch zones, counters).
void setDebugOverlayProvider(void (*fn)());
void debugText(float x, float y, const char* text, uint32_t argb = 0xFFFFFFFF);  // 8x8 SDL debug font
void strokeRect(float x, float y, float w, float h, uint32_t argb);
void strokeCircle(float cx, float cy, float r, uint32_t argb);
void present();
const Texture* snapshot();  // read back current frame (PlayState -snapshot:)
bool saveFrameBMP(const char* file);

// ---------------- GLES1 emulation used by the flixel engine ----------------
// Images are uploaded premultiplied into power-of-two textures, exactly like
// SemiSecretTexture's CGBitmapContext(kCGImageAlphaPremultipliedLast) path.
bool loadImageRGBA(const std::string& name, std::vector<uint8_t>& rgba, int& w, int& h);  // straight alpha
Texture* createTexture(const uint8_t* premultipliedRGBA, int w, int h, int paddedW, int paddedH, bool linear);
void destroyTexture(Texture* t);

// glMatrixMode(GL_MODELVIEW) stack, 2D affine. Projection = ortho(0..backing/zoom).
void glPushMatrix();
void glPopMatrix();
void glLoadIdentity();
void glTranslatef(float x, float y);
void glScalef(float x, float y);
void glRotatef(float deg);
void setProjectionZoom(float zoom);  // glOrthof(0, backingW/zoom, backingH/zoom, 0)
void setClearColor(float r, float g, float b, float a);
void glClear();

// glDrawArrays(GL_TRIANGLE_STRIP): positions in model space, uv normalized [0..1] of padded texture,
// colors premultiplied RGBA bytes per vertex (nullptr -> opaque white current color).
void drawStrip(const Texture* t, int n, const float* xy, const float* uv, const uint8_t* rgba, Blend blend);

// Render-to-texture (FlxSprite drawWithParam1: / bitmap baking). Target size w x h, ortho 0..w,0..h.
Texture* createRenderTarget(int w, int h);
void beginRenderTo(Texture* t);
void endRenderTo();

// --- input ---
const Touch& touch();

// --- audio (OpenAL semantics: one source per sound file; play restarts it) ---
void playSound(const std::string& name, float gain = 1.0f, bool looped = false);
void stopSound(const std::string& name);
bool soundPlaying(const std::string& name);
bool loadSound(const std::string& name);
// AVAudioPlayer semantics: one music player, numberOfLoops = -1
bool playMusic(const std::string& name, float volume = 1.0f);
void setMusicVolume(float v);
void stopMusic();
void pauseMusic();
void unpauseMusic();
bool musicPlaying();
void fadeOutMusic(float seconds);  // smoke-test helper; FlxG implements its own fade
void setMasterVolumes(float music, float sfx);  // config multipliers (same as AudioSettings music/sfx)
void updateAudio(float elapsed);

// --- persistence (NSUserDefaults replacement) ---
std::string prefPath(const std::string& file);
std::string assetPath(const std::string& file);
// Loose-file overrides: when set, assetPath() prefers <dir>/<file> if it exists ("" = off).
void setOverrideDir(const std::string& dir);

}  // namespace plat
