#include "core/ArtworkGen.h"
#include "core/FileIO.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <map>

#include "stb_image.h"

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#include "stb_image_write.h"

#include "../../res/icon_png.inc"  // the port's own "Z" icon (64 px PNG)
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace fs = std::filesystem;

namespace zig {

const char* const ArtworkGen::kFiles[5] = {"portrait.png", "wide.png", "hero.png", "logo.png", "icon.png"};

namespace {

struct Img {
    int w = 0, h = 0;
    std::vector<uint8_t> px;  // RGBA, straight alpha
    bool ok() const { return w > 0; }
    uint8_t* at(int x, int y) { return &px[((size_t)y * w + x) * 4]; }
    const uint8_t* at(int x, int y) const { return &px[((size_t)y * w + x) * 4]; }
};

Img blank(int w, int h, uint32_t rgba = 0) {
    Img i;
    i.w = w;
    i.h = h;
    i.px.resize((size_t)w * h * 4);
    for (size_t k = 0; k < (size_t)w * h; k++) {
        i.px[k * 4 + 0] = (uint8_t)(rgba >> 24);
        i.px[k * 4 + 1] = (uint8_t)(rgba >> 16);
        i.px[k * 4 + 2] = (uint8_t)(rgba >> 8);
        i.px[k * 4 + 3] = (uint8_t)rgba;
    }
    return i;
}

class Assets {
public:
    explicit Assets(std::string dir) : dir_(std::move(dir)) {}
    const Img& get(const std::string& name) {
        auto it = cache_.find(name);
        if (it != cache_.end()) return it->second;
        Img i;
        int n;
        std::vector<uint8_t> bytes;
        unsigned char* p = (zig::readFile(dir_ + "/" + name, bytes) && !bytes.empty())
            ? stbi_load_from_memory(bytes.data(), (int)bytes.size(), &i.w, &i.h, &n, 4) : nullptr;
        if (p) {
            i.px.assign(p, p + (size_t)i.w * i.h * 4);
            stbi_image_free(p);
        } else {
            i.w = i.h = 0;
            missing_.push_back(name);
        }
        return cache_[name] = std::move(i);
    }
    const std::vector<std::string>& missing() const { return missing_; }
private:
    std::string dir_;
    std::map<std::string, Img> cache_;
    std::vector<std::string> missing_;
};

// Alpha-blend a region of src onto dst at (dx,dy). flip mirrors horizontally. `alpha` multiplies.
void blit(Img& dst, const Img& src, int dx, int dy, int sx = 0, int sy = 0, int sw = -1, int sh = -1, bool flip = false,
          float alpha = 1.0f) {
    if (!src.ok()) return;
    if (sw < 0) sw = src.w;
    if (sh < 0) sh = src.h;
    for (int y = 0; y < sh; y++) {
        int ty = dy + y;
        if (ty < 0 || ty >= dst.h || sy + y >= src.h) continue;
        for (int x = 0; x < sw; x++) {
            int tx = dx + x;
            if (tx < 0 || tx >= dst.w) continue;
            int ux = flip ? sx + sw - 1 - x : sx + x;
            if (ux < 0 || ux >= src.w) continue;
            const uint8_t* s = src.at(ux, sy + y);
            float a = s[3] / 255.0f * alpha;
            if (a <= 0) continue;
            uint8_t* d = dst.at(tx, ty);
            float da = d[3] / 255.0f, oa = a + da * (1 - a);
            for (int c = 0; c < 3; c++)
                d[c] = (uint8_t)std::lround((s[c] * a + d[c] * da * (1 - a)) / (oa > 0 ? oa : 1));
            d[3] = (uint8_t)std::lround(oa * 255);
        }
    }
}

// Tile src horizontally across the whole width at row y, scrolled by offset.
void tileX(Img& dst, const Img& src, int y, int offset, float alpha = 1.0f) {
    if (!src.ok()) return;
    for (int x = -((offset % src.w + src.w) % src.w); x < dst.w; x += src.w) blit(dst, src, x, y, 0, 0, -1, -1, false, alpha);
}

Img upscale(const Img& s, int k) {
    Img d = blank(s.w * k, s.h * k);
    for (int y = 0; y < d.h; y++)
        for (int x = 0; x < d.w; x++) memcpy(d.at(x, y), s.at(x / k, y / k), 4);
    return d;
}

Img downscaleBox(const Img& s, int w, int h) {
    Img d = blank(w, h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int x0 = x * s.w / w, x1 = std::max(x0 + 1, (x + 1) * s.w / w);
            int y0 = y * s.h / h, y1 = std::max(y0 + 1, (y + 1) * s.h / h);
            double acc[4] = {0, 0, 0, 0};
            int n = 0;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++, n++)
                    for (int c = 0; c < 4; c++) acc[c] += s.at(xx, yy)[c];
            for (int c = 0; c < 4; c++) d.at(x, y)[c] = (uint8_t)std::lround(acc[c] / n);
        }
    return d;
}

// Vertical darkening towards the bottom so the logo/Steam UI stays readable.
void vignetteBottom(Img& d, int fromY, float maxDark) {
    for (int y = std::max(0, fromY); y < d.h; y++) {
        float t = (float)(y - fromY) / std::max(1, d.h - fromY) * maxDark;
        for (int x = 0; x < d.w; x++) {
            uint8_t* p = d.at(x, y);
            for (int c = 0; c < 3; c++) p[c] = (uint8_t)(p[c] * (1 - t));
        }
    }
}

bool save(const Img& i, const std::string& path) {
    std::vector<uint8_t> out;
    auto sink = [](void* ctx, void* data, int size) {
        auto* v = (std::vector<uint8_t>*)ctx;
        v->insert(v->end(), (uint8_t*)data, (uint8_t*)data + size);
    };
    if (!stbi_write_png_to_func(sink, &out, i.w, i.h, 4, i.px.data(), i.w * 4)) return false;
    std::ofstream f(fs::u8path(path), std::ios::binary | std::ios::trunc);
    f.write((const char*)out.data(), (std::streamsize)out.size());
    return (bool)f;
}

// An alien = body frame (28x30 sheet) + head sprite on top (heads: 12x10 .. 20x18).
void alien(Img& d, Assets& a, int x, int y, int frame, bool flip, const char* body = "img_AlienBody.png",
           const char* head = "img_AlienHead_size02.png") {
    const Img& b = a.get(body);
    int fw = b.w / 3, fh = b.h / 3;
    blit(d, b, x, y, (frame % 3) * fw, (frame / 3) * fh, fw, fh, flip);
    const Img& h = a.get(head);
    if (h.ok()) blit(d, h, x + (fw - h.w) / 2, y - h.h + 4, 0, 0, -1, -1, flip);
}

// The ziggurat + hero, centred at cx, with the zig top at topY.
void zigAndHero(Img& d, Assets& a, int cx, int topY) {
    const Img& z = a.get("mainZig.png");
    blit(d, z, cx - z.w / 2, topY);
    const Img& hero = a.get("new_Hero.png");  // 72x72 sheet, 24x24 frames
    blit(d, hero, cx - 12, topY - 18, 0, 0, 24, 24);
    const Img& gun = a.get("new_Gun.png");
    if (gun.ok()) blit(d, gun, cx + 4, topY - 6, 0, 0, std::min(gun.w, 22), std::min(gun.h, 8));
}

Img sunsetScene(Assets& a, int w, int h, int zigTop, int skyOffX, int skyOffY) {
    Img d = blank(w, h, 0x000000ff);
    blit(d, a.get("sky_sunset.png"), skyOffX, skyOffY);
    tileX(d, a.get("clouds4_sunset.png"), zigTop + 8, 40);
    tileX(d, a.get("clouds3_sunset.png"), zigTop + 22, 300);
    tileX(d, a.get("clouds2_sunset.png"), zigTop + 40, 150);
    zigAndHero(d, a, w / 2, zigTop);
    // aliens climbing on both sides (the zig steps: ~x +-60 per 26px of height)
    alien(d, a, w / 2 - 92, zigTop + 22, 1, false);
    alien(d, a, w / 2 + 64, zigTop + 22, 4, true);
    alien(d, a, w / 2 - 150, zigTop + 50, 6, false);
    alien(d, a, w / 2 + 122, zigTop + 50, 2, true);
    tileX(d, a.get("mainClouds.png"), h - 30, 0);
    return d;
}

Img nightScene(Assets& a, int w, int h) {
    Img d = blank(w, h, 0x000000ff);
    const Img& sky = a.get("sky_night.png");
    for (int x = 0; x < w; x += std::max(1, sky.w)) blit(d, sky, x, h - sky.h + 40);
    const Img& stars = a.get("stars.png");
    for (int x = 0; x < w; x += std::max(1, stars.w)) blit(d, stars, x, -60);
    const Img& moon = a.get("moon.png");
    blit(d, moon, w - 120, 6);
    const Img& ufo = a.get("img_UFO.png");  // 3x2 sheet of 128x62
    if (ufo.ok()) blit(d, ufo, 70, 14, 0, 0, ufo.w / 3, ufo.h / 2);
    tileX(d, a.get("clouds4_night.png"), h - 70, 80);
    tileX(d, a.get("clouds3_night.png"), h - 58, 260);
    const Img& z = a.get("mainZig.png");
    blit(d, z, w / 2 - z.w / 2, h - 66);
    const Img& hero = a.get("new_Hero.png");
    blit(d, hero, w / 2 - 12, h - 84, 24, 0, 24, 24);
    tileX(d, a.get("clouds2_night.png"), h - 34, 500);
    tileX(d, a.get("mainCloudsNight.png"), h - 22, 0);
    return d;
}

}  // namespace

ArtworkResult ArtworkGen::generate(const std::string& assetsDir, const std::string& outDir) {
    ArtworkResult r;
    std::error_code ec;
    fs::create_directories(fs::u8path(outDir), ec);
    Assets a(assetsDir);
    if (!a.get("mainZig.png").ok() || !a.get("ziglogo.png").ok()) {
        r.error = "game assets not found in " + assetsDir;
        return r;
    }
    const Img& logo = a.get("ziglogo.png");

    // portrait 600x900: scene 300x450 x2, logo x1 on top
    {
        Img s = sunsetScene(a, 300, 450, 286, -6, 36);
        // dark upper sky: the game's star field over the sunset image's dark top
        const Img& stars = a.get("stars.png");
        blit(s, stars, -100, -150, 0, 0, -1, -1, false, 0.9f);
        // cover the ziggurat's base with the foreground cloud bank (two rows)
        tileX(s, a.get("mainClouds.png"), 450 - 66, 120);
        tileX(s, a.get("mainClouds.png"), 450 - 40, 0);
        Img big = upscale(s, 2);
        blit(big, logo, (big.w - logo.w) / 2, 70);
        if (save(big, outDir + "/portrait.png")) r.files.push_back(outDir + "/portrait.png");
    }
    // wide 920x430: scene 460x215 x2, logo x1
    {
        Img s = sunsetScene(a, 460, 215, 112, -26, -110);
        Img big = upscale(s, 2);
        blit(big, logo, (big.w - logo.w) / 2, 22);
        if (save(big, outDir + "/wide.png")) r.files.push_back(outDir + "/wide.png");
    }
    // hero 1920x620: night scene 480x155 x4, no logo (Steam draws the logo on top)
    {
        Img s = nightScene(a, 480, 155);
        Img big = upscale(s, 4);
        if (save(big, outDir + "/hero.png")) r.files.push_back(outDir + "/hero.png");
    }
    // logo 768x170: ziglogo x2, transparent background
    {
        Img big = upscale(logo, 2);
        if (save(big, outDir + "/logo.png")) r.files.push_back(outDir + "/logo.png");
    }
    // icon 256x256: the port's own pixel-art "Z" (tools/make_icon.py), 64 px source x4 nearest -
    // the same icon as ziggurat.exe / ZigguratLauncher.exe. Falls back to the game's App Store art.
    {
        Img icon;
        int w = 0, h = 0, n = 0;
        unsigned char* px = stbi_load_from_memory(kIconPng, (int)sizeof kIconPng, &w, &h, &n, 4);
        if (px) {
            Img small;
            small.w = w;
            small.h = h;
            small.px.assign(px, px + (size_t)w * h * 4);
            stbi_image_free(px);
            icon = upscale(small, 256 / w);
        } else if (const Img& art = a.get("iTunesArtwork.jpg"); art.ok()) {
            icon = downscaleBox(art, 256, 256);
        } else {
            const Img& small = a.get("Icon.png");
            icon = blank(256, 256, 0x000000ff);
            if (small.ok()) {
                Img up = upscale(small, 4);
                blit(icon, up, (256 - up.w) / 2, (256 - up.h) / 2);
            }
        }
        if (save(icon, outDir + "/icon.png")) r.files.push_back(outDir + "/icon.png");
    }
    r.ok = r.files.size() == 5;
    if (!r.ok) r.error = "could not write all artwork files to " + outDir;
    for (auto& m : a.missing()) SDL_Log("artwork: optional asset missing: %s", m.c_str());
    return r;
}

}  // namespace zig
