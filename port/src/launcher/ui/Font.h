// ZigUI pixel text: stb_truetype, em-based sizing like the engine's SemiSecretFont
// (scale = size / unitsPerEm), alpha thresholded for crisp pixels, glyphs cached per size.
// Glyph coverage (L9): if the font lacks a character, or draws an accented letter exactly like
// its base letter (Flixel.ttf maps Ñ/Ç/Ê... to the plain letters), the glyph is COMPOSED from the
// base letter + a pixel accent; as a last resort the fallback font's glyph is used.
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "launcher/ui/Gfx.h"

namespace zui {

enum class Align { Left, Center, Right };

enum class GlyphSource { Native, Composed, Fallback, Missing };

class Font {
public:
    bool load(const std::string& ttfPath);
    bool loaded() const { return !data_.empty(); }
    // Pixel extents of a single line.
    int width(const std::string& utf8, float size) const;
    int lineHeight(float size) const;
    int ascent(float size) const;
    // Draw one line; (x,y) = top-left of the line box. Returns drawn width.
    int draw(const std::string& utf8, int x, int y, float size, Color c, Color shadow = Color(0, 0, 0, 0)) const;
    // Word wrap to maxWidth; '\n' forces breaks.
    std::vector<std::string> wrap(const std::string& utf8, float size, int maxWidth) const;
    // Extra horizontal spacing (px) between glyphs, per font (Silkscreen is tight).
    float tracking = 0;
    // Size multiplier so fonts with different em grids look alike (fallback font tuning).
    float sizeScale = 1.0f;
    // Font used for characters this one can neither draw nor compose (non-owning).
    const Font* fallback = nullptr;
    // How a character is rendered at this size (glyph-coverage test).
    GlyphSource source(uint32_t cp, float size) const;

    struct Raster {
        std::vector<unsigned char> a;  // thresholded alpha, w*h
        int w = 0, h = 0, xoff = 0, yoff = 0, adv = 0;
        bool exists = false;           // the font has a glyph for this codepoint
    };
    Raster raster(uint32_t cp, float size) const;

private:
    struct Glyph {
        Tex tex;
        int xoff = 0, yoff = 0;
        int adv = 0;
        GlyphSource src = GlyphSource::Native;
    };
    const Glyph& glyph(uint32_t cp, float size) const;
    Glyph makeGlyph(uint32_t cp, float size) const;
    std::vector<unsigned char> data_;
    std::shared_ptr<void> info_;  // stbtt_fontinfo
    mutable std::map<std::pair<int, uint32_t>, Glyph> cache_;
};

std::vector<uint32_t> utf8Decode(const std::string& s);

}  // namespace zui
