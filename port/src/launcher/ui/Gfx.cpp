#include "core/AppIcon.h"
#include "core/FileIO.h"
#include "launcher/ui/Gfx.h"

#include <cstring>

#include "stb_image.h"  // implementation lives in zigengine (platform.cpp)

namespace zui {

Gfx& gfx() {
    static Gfx g;
    return g;
}

bool Image::load(const std::string& path) {
    int n = 0;
    std::vector<uint8_t> bytes;
    unsigned char* d = (zig::readFile(path, bytes) && !bytes.empty())
        ? stbi_load_from_memory(bytes.data(), (int)bytes.size(), &w, &h, &n, 4) : nullptr;
    if (!d) {
        w = h = 0;
        px.clear();
        return false;
    }
    px.assign(d, d + (size_t)w * h * 4);
    stbi_image_free(d);
    return true;
}

Image Image::crop(int x, int y, int cw, int ch) const {
    Image o;
    o.w = cw;
    o.h = ch;
    o.px.assign((size_t)cw * ch * 4, 0);
    for (int j = 0; j < ch; j++)
        for (int i = 0; i < cw; i++) {
            int sx = x + i, sy = y + j;
            if (sx < 0 || sy < 0 || sx >= w || sy >= h) continue;
            memcpy(&o.px[(size_t)(j * cw + i) * 4], at(sx, sy), 4);
        }
    return o;
}

bool Gfx::init(const char* title, int windowScale, bool fullscreen) {
    if (windowScale < 1) windowScale = 1;
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (!SDL_CreateWindowAndRenderer(title, kW * windowScale, kH * windowScale, flags, &win_, &ren_)) {
        SDL_Log("zui: window: %s", SDL_GetError());
        return false;
    }
    zig::setWindowIcon(win_);
    SDL_SetWindowMinimumSize(win_, kW, kH);
    SDL_SetRenderVSync(ren_, 1);
    SDL_SetRenderLogicalPresentation(ren_, kW, kH, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);
    canvas_ = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, kW, kH);
    SDL_SetTextureScaleMode(canvas_, SDL_SCALEMODE_NEAREST);
    if (fullscreen) toggleFullscreen();
    return true;
}

void Gfx::shutdown() {
    if (canvas_) SDL_DestroyTexture(canvas_);
    if (ren_) SDL_DestroyRenderer(ren_);
    if (win_) SDL_DestroyWindow(win_);
    canvas_ = nullptr;
    ren_ = nullptr;
    win_ = nullptr;
}

Tex Gfx::texture(const Image& img) {
    Tex t;
    if (img.w <= 0 || img.h <= 0) return t;
    SDL_Surface* s = SDL_CreateSurfaceFrom(img.w, img.h, SDL_PIXELFORMAT_ABGR8888, (void*)img.px.data(), img.w * 4);
    t.t = SDL_CreateTextureFromSurface(ren_, s);
    SDL_DestroySurface(s);
    if (t.t) {
        SDL_SetTextureScaleMode(t.t, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(t.t, SDL_BLENDMODE_BLEND);
        t.w = img.w;
        t.h = img.h;
    }
    return t;
}

Tex Gfx::loadTexture(const std::string& path) {
    Image img;
    if (!img.load(path)) return {};
    return texture(img);
}

void Gfx::destroy(Tex& t) {
    if (t.t) SDL_DestroyTexture(t.t);
    t = {};
}

void Gfx::begin() {
    SDL_SetRenderTarget(ren_, canvas_);
    SDL_SetRenderDrawColor(ren_, 0, 0, 0, 255);
    SDL_RenderClear(ren_);
}

void Gfx::present() {
    SDL_SetRenderTarget(ren_, nullptr);
    SDL_SetRenderDrawColor(ren_, 0, 0, 0, 255);
    SDL_RenderClear(ren_);
    SDL_FRect dst{0, 0, (float)kW, (float)kH};
    SDL_RenderTexture(ren_, canvas_, nullptr, &dst);
    SDL_RenderPresent(ren_);
}

void Gfx::fill(Rect r, Color c) {
    if (r.w <= 0 || r.h <= 0 || c.a == 0) return;
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren_, c.r, c.g, c.b, c.a);
    SDL_FRect f{(float)r.x, (float)r.y, (float)r.w, (float)r.h};
    SDL_RenderFillRect(ren_, &f);
}

void Gfx::line(int x0, int y0, int x1, int y1, Color c) {
    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren_, c.r, c.g, c.b, c.a);
    SDL_RenderLine(ren_, (float)x0, (float)y0, (float)x1, (float)y1);
}

void Gfx::draw(const Tex& t, int x, int y, Color tint, bool flipX) {
    if (!t) return;
    drawRegion(t, {0, 0, t.w, t.h}, {x, y, t.w, t.h}, tint, flipX);
}

void Gfx::drawRegion(const Tex& t, Rect src, Rect dst, Color tint, bool flipX) {
    if (!t) return;
    SDL_SetTextureColorMod(t.t, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(t.t, tint.a);
    SDL_FRect s{(float)src.x, (float)src.y, (float)src.w, (float)src.h};
    SDL_FRect d{(float)dst.x, (float)dst.y, (float)dst.w, (float)dst.h};
    SDL_RenderTextureRotated(ren_, t.t, &s, &d, 0, nullptr, flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

void Gfx::setClip(const Rect* r) {
    if (!r) {
        SDL_SetRenderClipRect(ren_, nullptr);
        return;
    }
    SDL_Rect c{r->x, r->y, r->w, r->h};
    SDL_SetRenderClipRect(ren_, &c);
}

SDL_Surface* Gfx::readCanvas() {
    SDL_Texture* prev = SDL_GetRenderTarget(ren_);
    SDL_SetRenderTarget(ren_, canvas_);
    SDL_Surface* s = SDL_RenderReadPixels(ren_, nullptr);
    SDL_SetRenderTarget(ren_, prev);
    return s;
}

bool Gfx::saveCanvasBMP(const std::string& file) {
    SDL_Surface* s = readCanvas();
    if (!s) return false;
    bool ok = SDL_SaveBMP(s, file.c_str());
    SDL_DestroySurface(s);
    return ok;
}

Tex Gfx::canvasSnapshot() {
    Tex t;
    SDL_Surface* s = readCanvas();
    if (!s) return t;
    t.t = SDL_CreateTextureFromSurface(ren_, s);
    t.w = s->w;
    t.h = s->h;
    SDL_DestroySurface(s);
    if (t.t) {
        SDL_SetTextureScaleMode(t.t, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(t.t, SDL_BLENDMODE_BLEND);
    }
    return t;
}

void Gfx::toggleFullscreen() {
    fullscreen_ = !fullscreen_;
    SDL_SetWindowFullscreen(win_, fullscreen_);
}

void Gfx::toCanvas(float wx, float wy, float& cx, float& cy) {
    SDL_RenderCoordinatesFromWindow(ren_, wx, wy, &cx, &cy);
}

}  // namespace zui
