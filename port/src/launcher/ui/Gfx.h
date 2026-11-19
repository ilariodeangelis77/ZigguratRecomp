// ZigUI graphics layer: own SDL window/renderer with a fixed 480x320 canvas,
// integer-scaled + nearest filtering (same presentation as the game).
#pragma once
#include <SDL3/SDL.h>

#include <cstdint>
#include <string>
#include <vector>

namespace zui {

constexpr int kW = 480, kH = 320;

struct Color {
    uint8_t r = 255, g = 255, b = 255, a = 255;
    constexpr Color() = default;
    constexpr Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255) : r(r_), g(g_), b(b_), a(a_) {}
    static constexpr Color hex(uint32_t rgb, uint8_t a = 255) {
        return Color((uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb, a);
    }
    Color withAlpha(uint8_t na) const { return Color(r, g, b, na); }
};

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    Rect inset(int d) const { return {x + d, y + d, w - 2 * d, h - 2 * d}; }
    int cx() const { return x + w / 2; }
    int cy() const { return y + h / 2; }
};

// CPU-side RGBA image (straight alpha).
struct Image {
    int w = 0, h = 0;
    std::vector<uint8_t> px;  // RGBA
    bool load(const std::string& path);
    uint8_t* at(int x, int y) { return &px[(size_t)(y * w + x) * 4]; }
    const uint8_t* at(int x, int y) const { return &px[(size_t)(y * w + x) * 4]; }
    Image crop(int x, int y, int cw, int ch) const;
};

struct Tex {
    SDL_Texture* t = nullptr;
    int w = 0, h = 0;
    explicit operator bool() const { return t != nullptr; }
};

class Gfx {
public:
    bool init(const char* title, int windowScale, bool fullscreen);
    void shutdown();
    SDL_Window* window() { return win_; }
    SDL_Renderer* renderer() { return ren_; }

    Tex texture(const Image& img);               // upload (nearest filtering)
    Tex loadTexture(const std::string& path);    // PNG
    void destroy(Tex& t);

    void begin();                                // render into the 480x320 canvas
    void present();
    void fill(Rect r, Color c);
    void line(int x0, int y0, int x1, int y1, Color c);
    void draw(const Tex& t, int x, int y, Color tint = {}, bool flipX = false);
    void drawRegion(const Tex& t, Rect src, Rect dst, Color tint = {}, bool flipX = false);
    void setClip(const Rect* r);

    // Read back the canvas (480x320) — used for screenshots and the theme crossfade.
    SDL_Surface* readCanvas();
    bool saveCanvasBMP(const std::string& file);
    Tex canvasSnapshot();

    void toggleFullscreen();
    bool fullscreen() const { return fullscreen_; }
    // window coordinates -> canvas coordinates
    void toCanvas(float wx, float wy, float& cx, float& cy);

private:
    SDL_Window* win_ = nullptr;
    SDL_Renderer* ren_ = nullptr;
    SDL_Texture* canvas_ = nullptr;
    bool fullscreen_ = false;
};

Gfx& gfx();

}  // namespace zui
