#include "launcher/ui/Theme.h"
#include "core/TestHooks.h"

#include <set>

#include <algorithm>
#include <cmath>
#include <filesystem>
#define _USE_MATH_DEFINES
#include <math.h>

#include "launcher/ui/Audio.h"

namespace zui {

Theme& theme() {
    static Theme t;
    return t;
}

namespace {

bool exists(const std::string& p) {
    std::error_code ec;
    return std::filesystem::is_regular_file(std::filesystem::u8path(p), ec);
}

// Ice pixels: cyan face, teal shade, white glint (drop black outline and the red pillar).
bool icePixel(const uint8_t* p) { return p[3] > 128 && p[1] >= 100 && p[2] >= 100; }

// Split a strip of img_pillar.png into individual blocks separated by ice-free lines.
// horizontal=true: blocks along x (top/bottom rows). Result blocks keep only ice pixels.
std::vector<Image> splitStrip(const Image& img, Rect r, bool horizontal, int& thickness) {
    std::vector<Image> out;
    auto has = [&](int k) {
        for (int j = 0; j < (horizontal ? r.h : r.w); j++) {
            int x = horizontal ? r.x + k : r.x + j, y = horizontal ? r.y + j : r.y + k;
            if (icePixel(img.at(x, y))) return true;
        }
        return false;
    };
    // global thickness range
    int lo = 1 << 30, hi = -1;
    for (int k = 0; k < (horizontal ? r.w : r.h); k++)
        for (int j = 0; j < (horizontal ? r.h : r.w); j++) {
            int x = horizontal ? r.x + k : r.x + j, y = horizontal ? r.y + j : r.y + k;
            if (icePixel(img.at(x, y))) { lo = std::min(lo, j); hi = std::max(hi, j); }
        }
    if (hi < 0) return out;
    thickness = hi - lo + 1;
    int n = horizontal ? r.w : r.h;
    for (int k = 0; k < n;) {
        if (!has(k)) { k++; continue; }
        int s = k;
        while (k < n && has(k)) k++;
        Image b = horizontal ? img.crop(r.x + s, r.y + lo, k - s, thickness) : img.crop(r.x + lo, r.y + s, thickness, k - s);
        for (int y = 0; y < b.h; y++)
            for (int x = 0; x < b.w; x++)
                if (!icePixel(b.at(x, y))) b.at(x, y)[3] = 0;
        out.push_back(std::move(b));
    }
    return out;
}

// Procedural ice block in the game's style: rounded, cyan face, white glint, teal shade.
Image makeBlock(int w, int h, bool horizontal) {
    Image b;
    b.w = w;
    b.h = h;
    b.px.assign((size_t)w * h * 4, 0);
    auto put = [&](int x, int y, Color c) {
        uint8_t* p = b.at(x, y);
        p[0] = c.r; p[1] = c.g; p[2] = c.b; p[3] = 255;
    };
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            bool corner = (x == 0 || x == w - 1) && (y == 0 || y == h - 1);
            if (corner) continue;
            Color c = pal::ice;
            bool edge = x == 0 || y == 0 || x == w - 1 || y == h - 1;
            if (!edge) {
                if (horizontal) {
                    if (y <= std::max(1, h * 2 / 5) && x > 0 && x < w - 1) c = pal::iceGlint;
                    else if (y >= h - std::max(2, h * 2 / 5)) c = pal::iceShade;
                } else {
                    if (x >= w - std::max(2, w * 2 / 5)) c = pal::iceGlint;
                    else if (x <= std::max(1, w / 3)) c = pal::iceShade;
                }
            }
            put(x, y, c);
        }
    return b;
}

Image downscaleArrow(const Image& src, int fx, int fy, int fw, int fh, int k) {
    Image o;
    o.w = fw / k;
    o.h = fh / k;
    o.px.assign((size_t)o.w * o.h * 4, 0);
    for (int y = 0; y < o.h; y++)
        for (int x = 0; x < o.w; x++) {
            int cnt = 0, pinkN = 0;
            for (int j = 0; j < k; j++)
                for (int i = 0; i < k; i++) {
                    const uint8_t* p = src.at(fx + x * k + i, fy + y * k + j);
                    if (p[3] > 128) { cnt++; if (p[1] < 128) pinkN++; }
                }
            if (cnt * 2 >= k * k) {
                Color c = pinkN * 2 >= cnt ? pal::pink : pal::text;
                uint8_t* d = o.at(x, y);
                d[0] = c.r; d[1] = c.g; d[2] = c.b; d[3] = 255;
            }
        }
    return o;
}

Image proceduralChevron(int w, int h, int shift) {
    Image o;
    o.w = w;
    o.h = h;
    o.px.assign((size_t)w * h * 4, 0);
    auto put = [&](int x, int y, Color c) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        uint8_t* p = o.at(x, y);
        p[0] = c.r; p[1] = c.g; p[2] = c.b; p[3] = 255;
    };
    int mid = h / 2, th = std::max(2, h / 5);
    for (int c = 0; c < 2; c++) {
        int ox = shift + c * (w / 2);
        for (int y = 0; y < h; y++) {
            int d = std::abs(y - mid);
            for (int t = 0; t < th; t++) put(ox + d + t, y, t == 0 ? pal::text : pal::pink);
        }
    }
    return o;
}

}  // namespace

const Theme::BlockSet& Theme::set(FrameStyle s) const {
    int i = real() ? 1 : 0;
    return s == FrameStyle::Small ? small_[i] : big_[i];
}

const Font& Theme::font() const { return real() ? realFont_ : fallbackFont_; }

void Theme::useFallback(bool on) { forceFallback_ = on; }

void Theme::load(const std::string& assetsDir, const std::string& fallbackFont) {
    // Re-loadable (after a dump import): reset everything built by a previous load.
    for (int i = 0; i < 2; i++) {
        small_[i] = BlockSet{};
        big_[i] = BlockSet{};
        chev_[i].clear();
    }
    real_ = false;
    dir_ = assetsDir;
    if (!dir_.empty() && dir_.back() != '/' && dir_.back() != '\\') dir_ += '/';
    fallbackFont_.load(fallbackFont);
    fallbackFont_.sizeScale = 1.0f;
    buildFallback();
    real_ = exists(dir_ + "img_pillar.png") && exists(dir_ + "Flixel.ttf") && realFont_.load(dir_ + "Flixel.ttf");
    realFont_.fallback = &fallbackFont_;  // L9: characters Flixel.ttf can't draw or compose
    if (real_) buildReal(dir_);
}

void Theme::buildFallback() {
    // Small style: block sizes measured from img_pillar.png (rows 9px, columns 9px wide).
    auto& s = small_[0];
    s.rowH = 9; s.colW = 9; s.gap = 1; s.stick = 0;
    for (int w : {8, 10, 13, 8, 10, 12, 8, 10, 13, 8}) s.h.push_back({gfx().texture(makeBlock(w, 9, true)), w, 9});
    for (int h : {7, 10, 12}) s.v.push_back({gfx().texture(makeBlock(9, h, false)), 9, h});
    auto& b = big_[0];
    b.rowH = 18; b.colW = 18; b.gap = 2; b.stick = 6;
    for (int w : {16, 20, 25, 16, 20, 25, 16}) b.h.push_back({gfx().texture(makeBlock(w, 18, true)), w, 18});
    for (int h : {17, 21, 26}) b.v.push_back({gfx().texture(makeBlock(18, h, false)), 18, h});
    // Button: game palette gradient with lavender border.
    Image btn;
    btn.w = 126; btn.h = 40;
    btn.px.assign((size_t)126 * 40 * 4, 0);
    for (int y = 0; y < 40; y++)
        for (int x = 0; x < 126; x++) {
            bool cornerOut = (x == 0 || x == 125) && (y == 0 || y == 39);
            if (cornerOut) continue;
            Color c;
            if (y == 39) c = Color::hex(0x6600B6);
            else if (x == 0 || x == 125 || y == 0) {
                float k = y / 39.0f;
                c = Color((uint8_t)(0xD6 - k * (0xD6 - 0x3B)), (uint8_t)(0xD1 - k * (0xD1 - 0x24)), (uint8_t)(0xDA - k * (0xDA - 0x4D)));
            } else {
                float k = y / 38.0f;
                c = Color((uint8_t)(0x1B * (1 - k)), 0, (uint8_t)(0x30 * (1 - k)));
            }
            uint8_t* p = btn.at(x, y);
            p[0] = c.r; p[1] = c.g; p[2] = c.b; p[3] = 255;
        }
    btn_[0] = gfx().texture(btn);
    for (int f = 0; f < 10; f++) {
        int shift = (int)std::lround(2 * std::sin(f / 10.0 * 2 * M_PI));
        chev_[0].push_back(gfx().texture(proceduralChevron(23, 14, 2 + shift)));
    }
    arrow_[0] = gfx().texture(proceduralChevron(69, 42, 4));
}

void Theme::buildReal(const std::string& dir) {
    Image pillar;
    if (!pillar.load(dir + "img_pillar.png")) { real_ = false; return; }
    auto mk = [&](BlockSet& bs, Rect hRect, Rect vRect, int gap, int stick) {
        int t = 0;
        for (auto& im : splitStrip(pillar, hRect, true, t)) bs.h.push_back({gfx().texture(im), im.w, im.h});
        bs.rowH = t;
        for (auto& im : splitStrip(pillar, vRect, false, t)) bs.v.push_back({gfx().texture(im), im.w, im.h});
        bs.colW = t;
        bs.gap = gap;
        bs.stick = stick;
    };
    // Measured in img_pillar.png (see LAUNCHER_PLAN.md §4.3):
    //  small boxes ("COME GIOCARE"): top row x 72..224, y 229..239; left column x 72..80, y 239..272
    //  big box (main options frame): top row x 22..180 (left part, clear of the tab box), y 104..128;
    //                                left column x 16..33, y 126..193; columns stick out 6px.
    mk(small_[1], {72, 228, 153, 12}, {71, 239, 11, 34}, 1, 0);
    mk(big_[1], {22, 104, 159, 25}, {15, 127, 20, 66}, 2, 6);
    if (small_[1].h.empty() || small_[1].v.empty() || big_[1].h.empty() || big_[1].v.empty()) {
        SDL_Log("zui: could not cut ice blocks from img_pillar.png, using fallback frames");
        small_[1] = small_[0];
        big_[1] = big_[0];
    }
    btn_[1] = gfx().loadTexture(dir + "img_button.png");
    Image arrows;
    if (arrows.load(dir + "arrowSheet.png")) {
        arrow_[1] = gfx().texture(arrows);
        for (int f = 0; f < 10; f++) chev_[1].push_back(gfx().texture(downscaleArrow(arrows, (f % 5) * 69, (f / 5) * 42, 69, 42, 3)));
    }
    sky_ = gfx().loadTexture(dir + "sky_night.png");
    stars_ = gfx().loadTexture(dir + "stars.png");
    moon_ = gfx().loadTexture(dir + "moon.png");
    cloud2_ = gfx().loadTexture(dir + "clouds2_night.png");
    cloud3_ = gfx().loadTexture(dir + "clouds3_night.png");
    cloud4_ = gfx().loadTexture(dir + "clouds4_night.png");
    mainCloud_ = gfx().loadTexture(dir + "mainCloudsNight.png");
    logo_ = gfx().loadTexture(dir + "ziglogo.png");
    silhouette_ = gfx().loadTexture(dir + "img_silhouette.png");
}

std::vector<int> Theme::fill(const std::vector<Block>& blocks, int length, int gap, int& used) const {
    std::vector<int> seq;
    used = 0;
    if (blocks.empty() || length <= 0) return seq;
    // Exact fill: sum(w_i) + gap*(n-1) == L  <=>  sum(w_i + gap) == L + gap
    for (int L = length; L > 0; L--) {
        int T = L + gap;
        std::vector<char> reach(T + 1, 0);
        reach[0] = 1;
        for (int t = 1; t <= T; t++)
            for (auto& b : blocks)
                if (b.w + gap <= t && reach[t - (b.w + gap)]) { reach[t] = 1; break; }
        if (!reach[T]) continue;
        int R = T, cur = 0;
        while (R > 0) {
            bool found = false;
            for (size_t k = 0; k < blocks.size(); k++) {
                int i = (int)((cur + k) % blocks.size());
                int s = blocks[i].w + gap;
                if (s <= R && reach[R - s]) {
                    seq.push_back(i);
                    R -= s;
                    cur = i + 1;
                    found = true;
                    break;
                }
            }
            if (!found) break;
        }
        used = L;
        return seq;
    }
    return seq;
}

void Theme::frame(Rect r, FrameStyle style) const {
    const BlockSet& bs = set(style);
    gfx().fill(r, pal::ink);
    // rows
    int x0 = r.x + 1 + bs.stick, len = r.w - 2 - 2 * bs.stick, used = 0;
    std::vector<int> seq = fill(bs.h, len, bs.gap, used);
    int off = (len - used) / 2;
    auto drawRow = [&](int y, bool reverse) {
        int x = x0 + off;
        for (size_t k = 0; k < seq.size(); k++) {
            const Block& b = bs.h[seq[reverse ? seq.size() - 1 - k : k]];
            gfx().draw(b.tex, x, y);
            x += b.w + bs.gap;
        }
    };
    drawRow(r.y + 1, false);
    drawRow(r.y + r.h - 1 - bs.rowH, true);
    // columns
    int y0 = r.y + 1 + bs.rowH + bs.gap, vlen = (r.y + r.h - 1 - bs.rowH - bs.gap) - y0, vused = 0;
    std::vector<int> vseq;
    // columns use height as "width" for the fill solver
    std::vector<Block> vAsH;
    for (auto& b : bs.v) vAsH.push_back({b.tex, b.h, b.w});
    vseq = fill(vAsH, vlen, bs.gap, vused);
    int voff = (vlen - vused) / 2;
    auto drawCol = [&](int x, bool reverse) {
        int y = y0 + voff;
        for (size_t k = 0; k < vseq.size(); k++) {
            const Block& b = bs.v[vseq[reverse ? vseq.size() - 1 - k : k]];
            gfx().draw(b.tex, x, y);
            y += b.h + bs.gap;
        }
    };
    drawCol(r.x + 1, false);
    drawCol(r.x + r.w - 1 - bs.colW, true);
}

Rect Theme::frameContent(Rect r, FrameStyle style) const {
    const BlockSet& bs = set(style);
    int mx = 1 + bs.colW + 4, my = 1 + bs.rowH + 3;
    return {r.x + mx, r.y + my, r.w - 2 * mx, r.h - 2 * my};
}

void Theme::button(Rect r, bool focused, bool pressed) const {
    const Tex& t = btn_[real() ? 1 : 0];
    if (pressed) r.y += 1;
    if (t) {
        // 9-slice: 1px sides, 3px top/bottom; middle stretched (vertical gradient survives stretching).
        const int sx = 1, sy = 3;
        int mw = t.w - 2 * sx, mh = t.h - 2 * sy;
        int dw = r.w - 2 * sx, dh = r.h - 2 * sy;
        auto piece = [&](int srcx, int srcy, int sw, int sh, int dx, int dy, int dww, int dhh) {
            if (sw > 0 && sh > 0 && dww > 0 && dhh > 0) gfx().drawRegion(t, {srcx, srcy, sw, sh}, {dx, dy, dww, dhh});
        };
        piece(0, 0, sx, sy, r.x, r.y, sx, sy);
        piece(sx, 0, mw, sy, r.x + sx, r.y, dw, sy);
        piece(t.w - sx, 0, sx, sy, r.x + r.w - sx, r.y, sx, sy);
        piece(0, sy, sx, mh, r.x, r.y + sy, sx, dh);
        piece(sx, sy, mw, mh, r.x + sx, r.y + sy, dw, dh);
        piece(t.w - sx, sy, sx, mh, r.x + r.w - sx, r.y + sy, sx, dh);
        piece(0, t.h - sy, sx, sy, r.x, r.y + r.h - sy, sx, sy);
        piece(sx, t.h - sy, mw, sy, r.x + sx, r.y + r.h - sy, dw, sy);
        piece(t.w - sx, t.h - sy, sx, sy, r.x + r.w - sx, r.y + r.h - sy, sx, sy);
    }
    if (focused) {
        Color c = pal::select;
        gfx().fill({r.x + 1, r.y - 1, r.w - 2, 1}, c);
        gfx().fill({r.x + 1, r.y + r.h, r.w - 2, 1}, c);
        gfx().fill({r.x - 1, r.y + 1, 1, r.h - 2}, c);
        gfx().fill({r.x + r.w, r.y + 1, 1, r.h - 2}, c);
        gfx().fill({r.x, r.y, r.w, 1}, c);
        gfx().fill({r.x, r.y + r.h - 1, r.w, 1}, c);
        gfx().fill({r.x, r.y, 1, r.h}, c);
        gfx().fill({r.x + r.w - 1, r.y, 1, r.h}, c);
    }
}

void Theme::chevron(int x, int y, bool right, double t, Color) const {
    const auto& frames = chev_[real() && !chev_[1].empty() ? 1 : 0];
    if (frames.empty()) return;
    int f = (int)(t * 5.0) % (int)frames.size();  // "bounce" animation, 5 fps (TutorialState)
    gfx().draw(frames[f], x, y, Color(), right);
}

void Theme::bigArrow(int x, int y, bool right, double t) const {
    int f = (int)(t * 5.0) % 10;
    if (real() && arrow_[1]) gfx().drawRegion(arrow_[1], {(f % 5) * 69, (f / 5) * 42, 69, 42}, {x, y, 69, 42}, Color(), right);
    else {
        int dx = (int)std::lround(3 * std::sin(f / 10.0 * 2 * M_PI));
        gfx().draw(arrow_[0], x + (right ? -dx : dx), y, Color(), right);
    }
}

void Theme::background(double t) const {
    if (real() && sky_) {
        // StartState layout: sky centered, clouds at height-128/-115/-84, stars bottom, moon at (w-180, h-315),
        // main clouds at height-38. PORT/launcher: clouds scroll slowly (static in the original title).
        gfx().draw(sky_, kW / 2 - sky_.w / 2, kH / 2 - sky_.h / 2);
        auto scroll = [&](const Tex& tx, int y, double speed) {
            if (!tx) return;
            int x = -(int)std::fmod(t * speed, (double)tx.w);
            gfx().draw(tx, x, y);
            gfx().draw(tx, x + tx.w, y);
        };
        gfx().draw(stars_, kW / 2 - stars_.w / 2, kH - stars_.h);
        gfx().draw(moon_, kW - 180, kH - 315);
        scroll(cloud4_, kH - 128, 2.0);
        scroll(cloud3_, kH - 115, 4.0);
        scroll(cloud2_, kH - 84, 7.0);
        scroll(mainCloud_, kH - 38, 11.0);
        return;
    }
    // Procedural night sky in the game palette.
    const Color bands[] = {pal::ink, pal::night0, pal::night1, pal::night2, pal::night3, pal::night4};
    for (int i = 0; i < 6; i++) gfx().fill({0, i * 40, kW, 40}, bands[i]);
    gfx().fill({0, 240, kW, 80}, pal::night4);
    uint32_t seed = 12345;
    for (int i = 0; i < 70; i++) {
        seed = seed * 1103515245u + 12345u;
        int x = (seed >> 8) % kW;
        seed = seed * 1103515245u + 12345u;
        int y = (seed >> 8) % 200;
        bool tw = ((int)(t * 2) + i) % 7 == 0;
        gfx().fill({x, y, 1, 1}, tw ? pal::textDim : pal::text);
    }
    // moon
    int mx = kW - 140, my = 40, rr = 22;
    for (int y = -rr; y <= rr; y++) {
        int w = (int)std::sqrt((double)(rr * rr - y * y));
        gfx().fill({mx - w, my + y, 2 * w + 1, 1}, Color::hex(0xF4F0C8));
    }
    // cloud layers
    auto clouds = [&](int baseY, int step, int rad, Color c, double speed) {
        int off = (int)std::fmod(t * speed, (double)step);
        for (int x = -step - off; x < kW + step; x += step)
            for (int y = -rad; y <= rad; y++) {
                int w = (int)std::sqrt((double)(rad * rad - y * y));
                if (baseY + y >= baseY - rad / 2) gfx().fill({x - w, baseY + y, 2 * w, 1}, c);
            }
        gfx().fill({0, baseY + rad / 2, kW, kH - baseY}, c);
    };
    clouds(kH - 110, 46, 18, pal::night3, 3.0);
    clouds(kH - 70, 38, 16, pal::night2, 6.0);
    clouds(kH - 28, 30, 14, Color::hex(0x4B1878), 10.0);
}

void Theme::logo(int cx, int y) const {
    if (real() && logo_) {
        gfx().draw(logo_, cx - logo_.w / 2, y);
        return;
    }
    const Font& f = font();
    std::string s = "ZiGGURAT";
    int w = f.width(s, 32);
    f.draw(s, cx - w / 2 + 2, y + 2, 32, pal::zigRed);
    f.draw(s, cx - w / 2, y, 32, pal::logoYellow);
}

void Theme::silhouetteFlash(float a) const {
    uint8_t al = (uint8_t)std::clamp(a * 255.0f, 0.0f, 255.0f);
    if (al == 0) return;
    if (real() && silhouette_) {
        gfx().draw(silhouette_, kW / 2 - silhouette_.w / 2, kH - silhouette_.h, Color(255, 255, 255, al));
        return;
    }
    gfx().fill({0, 0, kW, kH}, Color(0xCF, 0x18, 0x20, al));
    for (int s = 0; s < 5; s++) gfx().fill({kW / 2 - 40 - s * 50, kH - 130 + s * 28, 80 + s * 100, 130}, Color(0, 0, 0, al));
}

// L9: ZUI_OVERFLOW_LOG=1 logs (once per string) any text wider than the space it is drawn in.
void Theme::fit(const char* ctx, const std::string& s, float size, int maxW) const {
    static const bool on = zig::testEnv("ZUI_OVERFLOW_LOG") != nullptr;
    if (!on || s.empty()) return;
    int w = font().width(s, size);
    if (w <= maxW) return;
    static std::set<std::string> seen;
    if (!seen.insert(std::string(ctx) + "|" + s).second) return;
    SDL_Log("ZUI-OVERFLOW [%s] %dpx > %dpx: %s", ctx, w, maxW, s.c_str());
}

void Theme::text(const std::string& s, int x, int y, float size, Color c, Align a, int boxW, bool sh) const {
    const Font& f = font();
    int w = f.width(s, size);
    if (boxW > 0) fit("box", s, size, boxW);
    int dx = a == Align::Center ? (boxW - w) / 2 : a == Align::Right ? boxW - w : 0;
    f.draw(s, x + dx, y, size, c, sh ? pal::shadow : Color(0, 0, 0, 0));
}

void Theme::sndMove() const { if (realAvailable()) audio::play(dir_ + "enemyShoot.wav", 0.25f); }
void Theme::sndConfirm() const { if (realAvailable()) audio::play(dir_ + "shoot.wav", 0.6f); }
void Theme::sndError() const { if (realAvailable()) audio::play(dir_ + "explode.wav", 0.5f); }
std::string Theme::musicPath() const { return realAvailable() ? dir_ + "Overture.mp3" : ""; }

}  // namespace zui
