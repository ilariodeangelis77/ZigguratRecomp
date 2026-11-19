#include "launcher/ui/Font.h"
#include "core/FileIO.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>

#include "stb_truetype.h"  // implementation lives in zigengine (engine/SemiSecret.cpp)

namespace zui {

std::vector<uint32_t> utf8Decode(const std::string& s) {
    std::vector<uint32_t> out;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        uint32_t cp;
        int n;
        if (c < 0x80) { cp = c; n = 1; }
        else if ((c >> 5) == 6) { cp = c & 31; n = 2; }
        else if ((c >> 4) == 14) { cp = c & 15; n = 3; }
        else { cp = c & 7; n = 4; }
        for (int k = 1; k < n && i + k < s.size(); k++) cp = (cp << 6) | ((unsigned char)s[i + k] & 63);
        out.push_back(cp);
        i += n;
    }
    return out;
}

static stbtt_fontinfo* fi(const std::shared_ptr<void>& p) { return (stbtt_fontinfo*)p.get(); }

bool Font::load(const std::string& path) {
    std::ifstream f(zig::u8p(path), std::ios::binary);
    if (!f) return false;
    data_.assign(std::istreambuf_iterator<char>(f), {});
    auto info = std::make_shared<stbtt_fontinfo>();
    if (!stbtt_InitFont(info.get(), data_.data(), stbtt_GetFontOffsetForIndex(data_.data(), 0))) {
        data_.clear();
        return false;
    }
    info_ = info;
    cache_.clear();
    return true;
}

static float emScale(stbtt_fontinfo* f, float size) { return stbtt_ScaleForMappingEmToPixels(f, size); }

int Font::ascent(float size) const {
    if (!loaded()) return (int)size;
    int a, d, g;
    stbtt_GetFontVMetrics(fi(info_), &a, &d, &g);
    return (int)std::lround(a * emScale(fi(info_), size * sizeScale));
}

int Font::lineHeight(float size) const {
    if (!loaded()) return (int)size;
    int a, d, g;
    stbtt_GetFontVMetrics(fi(info_), &a, &d, &g);
    return (int)std::lround((a - d + g) * emScale(fi(info_), size * sizeScale));
}

// ------------------------------------------------------------------ glyph coverage (L9)
namespace {

enum Accent { kNone, kAcute, kGrave, kCirc, kTilde, kDiaer, kRing, kCedilla };

// Latin-1 letters used by the 7 languages -> base letter + accent.
bool decompose(uint32_t cp, uint32_t& base, Accent& acc) {
    struct D { uint32_t cp, base; Accent a; };
    static const D tbl[] = {
        {0xC0, 'A', kGrave}, {0xC1, 'A', kAcute}, {0xC2, 'A', kCirc}, {0xC3, 'A', kTilde}, {0xC4, 'A', kDiaer},
        {0xC5, 'A', kRing}, {0xC7, 'C', kCedilla}, {0xC8, 'E', kGrave}, {0xC9, 'E', kAcute}, {0xCA, 'E', kCirc},
        {0xCB, 'E', kDiaer}, {0xCC, 'I', kGrave}, {0xCD, 'I', kAcute}, {0xCE, 'I', kCirc}, {0xCF, 'I', kDiaer},
        {0xD1, 'N', kTilde}, {0xD2, 'O', kGrave}, {0xD3, 'O', kAcute}, {0xD4, 'O', kCirc}, {0xD5, 'O', kTilde},
        {0xD6, 'O', kDiaer}, {0xD9, 'U', kGrave}, {0xDA, 'U', kAcute}, {0xDB, 'U', kCirc}, {0xDC, 'U', kDiaer},
        {0xDD, 'Y', kAcute}, {0xE0, 'a', kGrave}, {0xE1, 'a', kAcute}, {0xE2, 'a', kCirc}, {0xE3, 'a', kTilde},
        {0xE4, 'a', kDiaer}, {0xE5, 'a', kRing}, {0xE7, 'c', kCedilla}, {0xE8, 'e', kGrave}, {0xE9, 'e', kAcute},
        {0xEA, 'e', kCirc}, {0xEB, 'e', kDiaer}, {0xEC, 'i', kGrave}, {0xED, 'i', kAcute}, {0xEE, 'i', kCirc},
        {0xEF, 'i', kDiaer}, {0xF1, 'n', kTilde}, {0xF2, 'o', kGrave}, {0xF3, 'o', kAcute}, {0xF4, 'o', kCirc},
        {0xF5, 'o', kTilde}, {0xF6, 'o', kDiaer}, {0xF9, 'u', kGrave}, {0xFA, 'u', kAcute}, {0xFB, 'u', kCirc},
        {0xFC, 'u', kDiaer}, {0xFD, 'y', kAcute}, {0xFF, 'y', kDiaer},
    };
    for (const D& d : tbl)
        if (d.cp == cp) {
            base = d.base;
            acc = d.a;
            return true;
        }
    return false;
}

// Accent pixel patterns at 1 font-pixel scale ('#' = ink).
const std::vector<const char*>& pattern(Accent a) {
    static const std::vector<const char*> acute{".#", "#."}, grave{"#.", ".#"}, circ{".#.", "#.#"},
        tilde{".#.#", "#.#."}, diaer{"#.#"}, ring{".#.", "#.#", ".#."}, ced{".#", "##"}, none{};
    switch (a) {
        case kAcute: return acute;
        case kGrave: return grave;
        case kCirc: return circ;
        case kTilde: return tilde;
        case kDiaer: return diaer;
        case kRing: return ring;
        case kCedilla: return ced;
        default: return none;
    }
}

bool sameInk(const Font::Raster& a, const Font::Raster& b) {
    return a.w == b.w && a.h == b.h && a.xoff == b.xoff && a.yoff == b.yoff && a.a == b.a;
}

int inkHeight(const Font::Raster& r) {
    int top = -1, bottom = -1;
    for (int y = 0; y < r.h; y++)
        for (int x = 0; x < r.w; x++)
            if (r.a[(size_t)y * r.w + x]) {
                if (top < 0) top = y;
                bottom = y;
            }
    return top < 0 ? 0 : bottom - top + 1;
}

bool rowHasInk(const Font::Raster& r, int y) {
    for (int x = 0; x < r.w; x++)
        if (r.a[(size_t)y * r.w + x]) return true;
    return false;
}

// Base letter + pixel accent. Accent pixels scale with the font's pixel size (size/8, >= 1).
bool compose(const Font& font, uint32_t base, Accent acc, float size, Font::Raster& out) {
    Font::Raster b = font.raster(base, size);
    if (!b.exists || b.w == 0) return false;
    const auto& pat = pattern(acc);
    if (pat.empty()) return false;
    int u = std::max(1, (int)std::lround(size / 8.0f));
    int pw = (int)strlen(pat[0]) * u, ph = (int)pat.size() * u;
    int minX = b.w, maxX = -1, minY = b.h, maxY = -1;
    for (int y = 0; y < b.h; y++)
        for (int x = 0; x < b.w; x++)
            if (b.a[(size_t)y * b.w + x]) {
                minX = std::min(minX, x);
                maxX = std::max(maxX, x);
                minY = std::min(minY, y);
                maxY = std::max(maxY, y);
            }
    if (maxX < 0) return false;
    if (base == 'i' && acc != kCedilla) {
        // remove the dot of 'i': clear the first ink run of rows if a gap follows it
        int y = minY;
        while (y <= maxY && rowHasInk(b, y)) y++;
        if (y <= maxY) {
            for (int yy = minY; yy < y; yy++)
                for (int x = 0; x < b.w; x++) b.a[(size_t)yy * b.w + x] = 0;
            while (y <= maxY && !rowHasInk(b, y)) y++;
            minY = y;
        }
    }
    int cx = (minX + maxX + 1) / 2;
    int ax = cx - pw / 2;
    int ay = acc == kCedilla ? maxY + 1 : minY - u - ph;  // 1 px gap above; cedilla attached below
    int nx0 = std::min(0, ax), ny0 = std::min(0, ay);
    int nx1 = std::max(b.w, ax + pw), ny1 = std::max(b.h, ay + ph);
    out = Font::Raster();
    out.exists = true;
    out.w = nx1 - nx0;
    out.h = ny1 - ny0;
    out.xoff = b.xoff + nx0;
    out.yoff = b.yoff + ny0;
    out.adv = b.adv;
    out.a.assign((size_t)out.w * out.h, 0);
    for (int y = 0; y < b.h; y++)
        for (int x = 0; x < b.w; x++)
            if (b.a[(size_t)y * b.w + x]) out.a[(size_t)(y - ny0) * out.w + (x - nx0)] = 255;
    for (int py = 0; py < (int)pat.size(); py++)
        for (int px = 0; pat[py][px]; px++)
            if (pat[py][px] == '#')
                for (int dy = 0; dy < u; dy++)
                    for (int dx = 0; dx < u; dx++)
                        out.a[(size_t)(ay + py * u + dy - ny0) * out.w + (ax + px * u + dx - nx0)] = 255;
    return true;
}

}  // namespace

Font::Raster Font::raster(uint32_t cp, float size) const {
    Raster r;
    if (!loaded()) return r;
    stbtt_fontinfo* f = fi(info_);
    r.exists = stbtt_FindGlyphIndex(f, (int)cp) != 0;
    if (!r.exists) return r;
    float sc = emScale(f, size * sizeScale);
    int adv, lsb;
    stbtt_GetCodepointHMetrics(f, (int)cp, &adv, &lsb);
    r.adv = (int)std::lround(adv * sc + tracking);
    unsigned char* bm = stbtt_GetCodepointBitmap(f, sc, sc, (int)cp, &r.w, &r.h, &r.xoff, &r.yoff);
    if (bm && r.w > 0 && r.h > 0) {
        r.a.resize((size_t)r.w * r.h);
        for (int i = 0; i < r.w * r.h; i++) r.a[(size_t)i] = bm[i] >= 128 ? 255 : 0;  // crisp pixels
    } else {
        r.w = r.h = 0;
    }
    if (bm) stbtt_FreeBitmap(bm, nullptr);
    return r;
}

Font::Glyph Font::makeGlyph(uint32_t cp, float size) const {
    Glyph g;
    Raster r = raster(cp, size);
    uint32_t base = 0;
    Accent acc = kNone;
    bool decomp = decompose(cp, base, acc);
    bool usable = r.exists;
    if (usable && decomp) {
        // Flixel.ttf maps e.g. U+00D1 to the plain N glyph: treat that as missing.
        Raster b = raster(base, size);
        if (b.exists && sameInk(r, b)) usable = false;
        // Flixel.ttf draws accented CAPITALS with the lowercase glyph (Ñ == ñ): compose from the capital.
        if (usable && cp >= 0xC0 && cp <= 0xDD) {
            Raster lower = raster(cp + 0x20, size);
            if (lower.exists && sameInk(r, lower)) usable = false;
            // A real accented capital is taller than its base capital (accent on top of a full-size
            // letter). If it isn't, the font substituted a small letter: compose instead.
            if (usable && b.exists && acc != kCedilla && inkHeight(r) <= inkHeight(b)) usable = false;
        }
    }
    if (!usable && cp >= 0x80) {
        Raster c;
        if (decomp && compose(*this, base, acc, size, c)) {
            r = c;
            g.src = GlyphSource::Composed;
        } else if (fallback && fallback != this && fallback->loaded() && fallback->raster(cp, size).exists) {
            Glyph fg = fallback->glyph(cp, size);
            fg.src = GlyphSource::Fallback;
            return fg;
        } else {
            g.src = GlyphSource::Missing;
            if (!r.exists) r = raster('?', size);
        }
    }
    g.adv = r.adv;
    if (r.w > 0 && r.h > 0) {
        Image img;
        img.w = r.w;
        img.h = r.h;
        img.px.assign((size_t)r.w * r.h * 4, 255);
        for (int i = 0; i < r.w * r.h; i++) img.px[(size_t)i * 4 + 3] = r.a[(size_t)i];
        g.tex = gfx().texture(img);
        g.xoff = r.xoff;
        g.yoff = r.yoff;
    }
    return g;
}

const Font::Glyph& Font::glyph(uint32_t cp, float size) const {
    int key = (int)std::lround(size * 16);
    auto k = std::make_pair(key, cp);
    auto it = cache_.find(k);
    if (it != cache_.end()) return it->second;
    Glyph g = makeGlyph(cp, size);
    return cache_[k] = g;
}

GlyphSource Font::source(uint32_t cp, float size) const {
    if (!loaded()) return GlyphSource::Missing;
    return glyph(cp, size).src;
}

int Font::width(const std::string& s, float size) const {
    if (!loaded()) return 0;
    int w = 0;
    for (uint32_t cp : utf8Decode(s)) w += glyph(cp, size).adv;
    return w;
}

int Font::draw(const std::string& s, int x, int y, float size, Color c, Color shadow) const {
    if (!loaded()) return 0;
    int base = y + ascent(size);
    int pen = x;
    for (uint32_t cp : utf8Decode(s)) {
        const Glyph& g = glyph(cp, size);
        if (g.tex) {
            if (shadow.a) gfx().draw(g.tex, pen + g.xoff + 1, base + g.yoff + 1, shadow);
            gfx().draw(g.tex, pen + g.xoff, base + g.yoff, c);
        }
        pen += g.adv;
    }
    return pen - x;
}

std::vector<std::string> Font::wrap(const std::string& s, float size, int maxW) const {
    std::vector<std::string> lines;
    std::string para;
    auto flushPara = [&](const std::string& p) {
        std::string line, word;
        auto pushWord = [&]() {
            if (word.empty()) return;
            std::string cand = line.empty() ? word : line + " " + word;
            if (!line.empty() && width(cand, size) > maxW) {
                lines.push_back(line);
                line = word;
            } else line = cand;
            word.clear();
        };
        for (char ch : p) {
            if (ch == ' ') pushWord();
            else word += ch;
        }
        pushWord();
        lines.push_back(line);
    };
    size_t start = 0;
    while (true) {
        size_t nl = s.find('\n', start);
        flushPara(s.substr(start, nl == std::string::npos ? std::string::npos : nl - start));
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    return lines;
}

}  // namespace zui
