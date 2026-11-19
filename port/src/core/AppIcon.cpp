#include "core/AppIcon.h"

#include <SDL3/SDL.h>

#include "stb_image.h"  // implementation lives in core/StbImage.cpp

#include "../../res/icon_png.inc"

namespace zig {

void setWindowIcon(SDL_Window* window) {
    if (!window) return;
    int w = 0, h = 0, n = 0;
    unsigned char* px = stbi_load_from_memory(kIconPng, (int)sizeof kIconPng, &w, &h, &n, 4);
    if (!px) return;
    SDL_Surface* s = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ABGR8888, px, w * 4);
    if (s) {
        SDL_SetWindowIcon(window, s);
        SDL_DestroySurface(s);
    }
    stbi_image_free(px);
}

}  // namespace zig
