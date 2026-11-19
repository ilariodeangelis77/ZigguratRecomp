//
//  SemiSecret.cpp — SemiSecretTexture.m, SemiSecretFont.m, SemiSecretText.m, fonts/FlixelFont.m
//
//  Copyright Semi Secret Software 2009-2010. All rights reserved.
//  C++ port: MIT license (see ENGINE_LICENSE.txt).
//
//  PORT: CoreGraphics (CGBitmapContext, CGFont, CGContextShowGlyphsAtPoint) is replaced by
//  stb_truetype rasterising straight into the premultiplied RGBA buffer that SemiSecretTexture uploads.
//
#include <SDL3/SDL.h>
#include "core/FileIO.h"

#include <algorithm>
#include <climits>
#include <cstring>
#include <fstream>
#include <iterator>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "Flixel.h"

// ============================== SemiSecretTexture ==============================
static SSTextureFiltering textureFilteringMode_ = SSTextureFilteringDefault;
static SSTextureDepth textureDepth_ = SSTextureDepth32Bits;

void SemiSecretTexture::setTextureFilteringMode(SSTextureFiltering f) { textureFilteringMode_ = f; }
SSTextureFiltering SemiSecretTexture::textureFilteringMode() { return textureFilteringMode_; }
void SemiSecretTexture::setTextureDepth(SSTextureDepth d) { textureDepth_ = d; }
SSTextureDepth SemiSecretTexture::textureDepth() { return textureDepth_; }

SemiSecretTexture* SemiSecretTexture::textureWithImage(const std::string& filename) {
    auto* t = new SemiSecretTexture;
    if (!t->initWithImage(filename)) {
        delete t;
        return nullptr;
    }
    return t;
}
SemiSecretTexture* SemiSecretTexture::textureWithColor(unsigned int color) { return (new SemiSecretTexture)->initWithColor(color); }
SemiSecretTexture* SemiSecretTexture::textureWithView(SemiSecretText* view) { return (new SemiSecretTexture)->initWithView(view); }

// - (id) initWithImage:(NSString *)filename
SemiSecretTexture* SemiSecretTexture::initWithImage(const std::string& filename) {
    std::vector<uint8_t> img;
    int w, h;
    if (!plat::loadImageRGBA(filename, img, w, h)) {
        SDL_Log("SemiSecretTexture initWithImage: could not find image named: %s", filename.c_str());
        return nullptr;
    }
    prepareWithWidth(w, h);
    // CGContextDrawImage into a kCGImageAlphaPremultipliedLast context at the top of the padded buffer
    const int pw = (int)paddedSize.width;
    for (int yy = 0; yy < h; yy++) {
        const uint8_t* s = &img[(size_t)yy * w * 4];
        uint8_t* d = &data[(size_t)yy * pw * 4];
        for (int xx = 0; xx < w; xx++, s += 4, d += 4) {
            unsigned a = s[3];
            d[0] = (uint8_t)((s[0] * a + 127) / 255);
            d[1] = (uint8_t)((s[1] * a + 127) / 255);
            d[2] = (uint8_t)((s[2] * a + 127) / 255);
            d[3] = (uint8_t)a;
        }
    }
    createTexture();
    setupVerticesTexCoords();
    data.clear();
    return this;
}

// - (id) initWithView:(UIView *)view   (SemiSecretText -drawRect: rendered into the bitmap)
SemiSecretTexture* SemiSecretTexture::initWithView(SemiSecretText* view) {
    prepareWithWidth((int)view->bounds.size.width, (int)view->bounds.size.height);
    view->renderInto(data.data(), (int)paddedSize.width, (int)paddedSize.height);
    createTexture();
    setupVerticesTexCoords();
    data.clear();
    return this;
}

// - (id) initWithColor:(unsigned int)color
SemiSecretTexture* SemiSecretTexture::initWithColor(unsigned int color) {
    prepareWithWidth(8, 8);
    GLubyte r = (GLubyte)((color >> 16) & 0xff);
    GLubyte g = (GLubyte)((color >> 8) & 0xff);
    GLubyte b = (GLubyte)(color & 0xff);
    GLubyte a = (GLubyte)((color >> 24) & 0xff);
    for (int i = 0; i < size.width * size.height * 4; i += 4) {
        data[i] = r;
        data[i + 1] = g;
        data[i + 2] = b;
        data[i + 3] = a;
    }
    createTexture();
    setupVerticesTexCoords();
    data.clear();
    return this;
}

SemiSecretTexture::~SemiSecretTexture() {
    if (texture && !atlasTexture) plat::destroyTexture(texture);
}

// - (void) prepareWithWidth:(int)width height:(int)height;
void SemiSecretTexture::prepareWithWidth(int w, int h) {
    size = CGSizeMake(w, h);
    int paddedWidth = w;
    int paddedHeight = h;
    if (w != 1 && (w & (w - 1))) {
        paddedWidth = 1;
        while (paddedWidth < w) paddedWidth <<= 1;
    }
    if (h != 1 && (h & (h - 1))) {
        paddedHeight = 1;
        while (paddedHeight < h) paddedHeight <<= 1;
    }
    if (paddedWidth < 1) paddedWidth = 1;
    if (paddedHeight < 1) paddedHeight = 1;
    paddedSize = CGSizeMake(paddedWidth, paddedHeight);
    data.assign((size_t)paddedWidth * 4 * paddedHeight, 0);
    if (paddedWidth > 2048 || paddedHeight > 2048)
        SDL_Log("SemiSecretTexture: maximum texture size is %d, requested image is too large (%dx%d)", 2048, paddedWidth, paddedHeight);
}

// - (void) setAnimated:(BOOL)Animated
void SemiSecretTexture::setAnimated(BOOL Animated) {
    if (animated == Animated) return;
    animated = Animated;
    GLshort uShort = 0;
    GLshort vShort = 0;
    if (animated) {
        vertexData[2] = (GLshort)size.height;
        vertexData[6] = (GLshort)size.height;
        frames = (unsigned int)(size.width / size.height);
        uShort = FlxGLView::convertToShort(size.height / paddedSize.width);
        vShort = FlxGLView::convertToShort(size.height / paddedSize.height);
    } else {
        vertexData[2] = (GLshort)size.width;
        vertexData[6] = (GLshort)size.width;
        frames = 1;
        uShort = FlxGLView::convertToShort(size.width / paddedSize.width);
        vShort = FlxGLView::convertToShort(size.height / paddedSize.height);
    }
    texCoords.assign(8 * frames, 0);
    for (unsigned int i = 0; i < frames; ++i) {
        texCoords[i * 8 + 0] = i * uShort;
        texCoords[i * 8 + 1] = 0;
        texCoords[i * 8 + 2] = (i + 1) * uShort;
        texCoords[i * 8 + 3] = 0;
        texCoords[i * 8 + 4] = i * uShort;
        texCoords[i * 8 + 5] = vShort;
        texCoords[i * 8 + 6] = (i + 1) * uShort;
        texCoords[i * 8 + 7] = vShort;
    }
}

// - (void) setupVerticesTexCoords
void SemiSecretTexture::setupVerticesTexCoords() {
    vertexData[0] = 0;
    vertexData[1] = 0;
    vertexData[2] = (GLshort)size.width;
    vertexData[3] = 0;
    vertexData[4] = 0;
    vertexData[5] = (GLshort)size.height;
    vertexData[6] = (GLshort)size.width;
    vertexData[7] = (GLshort)size.height;
    texCoords.clear();
    animated = true;
    setAnimated(false);
}

// - (void) createTexture;   16-bit depths are emulated by quantising the 8-bit data the same way.
void SemiSecretTexture::createTexture() {
    const size_t n = (size_t)paddedSize.width * paddedSize.height * 4;
    if (textureDepth_ == SSTextureDepth16Bits5551) {
        for (size_t i = 0; i < n; i += 4) {
            for (int c = 0; c < 3; c++) {
                unsigned v = data[i + c] >> 3;
                data[i + c] = (GLubyte)((v << 3) | (v >> 2));
            }
            data[i + 3] = (data[i + 3] >> 7) ? 255 : 0;
        }
    } else if (textureDepth_ == SSTextureDepth16Bits4444) {
        for (size_t i = 0; i < n; i++) data[i] = (GLubyte)((data[i] >> 4) * 17);
    }
    bool linear;
    if (textureFilteringMode_ == SSTextureFilteringDefault)
        linear = FlxG::iPad() || FlxG::retinaDisplay();
    else
        linear = textureFilteringMode_ == SSTextureFilteringLinear;
    texture = plat::createTexture(data.data(), (int)size.width, (int)size.height, (int)paddedSize.width, (int)paddedSize.height, linear);
}

// ============================== Fonts ==============================
struct FontData {
    std::vector<unsigned char> file;
    stbtt_fontinfo info;
    int unitsPerEm = 2048;
    int capHeight = 0;
};

static FontData* flixelFont = nullptr;

static FontData* loadFont(const std::string& file) {
    std::ifstream f(zig::u8p(plat::assetPath(file)), std::ios::binary);
    if (!f) {
        SDL_Log("font %s not found", file.c_str());
        return nullptr;
    }
    auto* fd = new FontData;
    fd->file.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    if (!stbtt_InitFont(&fd->info, fd->file.data(), stbtt_GetFontOffsetForIndex(fd->file.data(), 0))) {
        delete fd;
        return nullptr;
    }
    fd->unitsPerEm = (int)std::lround(1.0 / stbtt_ScaleForMappingEmToPixels(&fd->info, 1.0f));
    // CGFontGetCapHeight: OS/2 v2+ sCapHeight. Flixel.ttf has OS/2 v0, so fall back to the 'H' outline.
    // PORT: verify against device screenshots; CoreGraphics' own fallback is undocumented.
    int x0, y0, x1, y1;
    if (stbtt_GetCodepointBox(&fd->info, 'H', &x0, &y0, &x1, &y1)) fd->capHeight = y1;
    else {
        int asc, desc, gap;
        stbtt_GetFontVMetrics(&fd->info, &asc, &desc, &gap);
        fd->capHeight = asc;
    }
    return fd;
}

static int glyphAdvance(FontData* fd, uint16_t glyph) {
    int adv, lsb;
    stbtt_GetGlyphHMetrics(&fd->info, glyph, &adv, &lsb);
    return adv;
}

// + (SemiSecretFont *)fontWithName:(NSString *)name size:(CGFloat) size;   (NSClassFromString(name+"Font"))
SemiSecretFont* SemiSecretFont::fontWithName(const std::string& name, float size) {
    if (name == "Flixel") return (new FlixelFont)->initWithSize(size);
    return nullptr;
}
SemiSecretFont* SemiSecretFont::fontWithSize(float s) { return (new SemiSecretFont)->initWithSize(s); }
// - (void) setGlyphs:(CGGlyph *)glyphs forCharacters:(unichar *)chars size:(NSUInteger)length
void SemiSecretFont::setGlyphs(uint16_t* glyphs, const unichar* chars, size_t length) {
    for (size_t i = 0; i < length; ++i) glyphs[i] = chars[i] - 29;
}
// - (CGSize) sizeToRenderString:(NSString *)string;
CGSize SemiSecretFont::sizeToRenderString(const std::u16string& string) {
    if (!font) return CGSizeMake(0, 0);
    std::vector<uint16_t> g(string.size());
    setGlyphs(g.data(), (const unichar*)string.data(), string.size());
    int total = 0;
    for (auto gl : g) total += glyphAdvance(font, gl);
    return CGSizeMake(total * size / font->unitsPerEm, font->capHeight * size / font->unitsPerEm);
}

SemiSecretFont* FlixelFont::initWithSize(float fontsize) {
    SemiSecretFont::initWithSize(fontsize);
    if (!flixelFont) flixelFont = loadFont("Flixel.ttf");
    font = flixelFont;
    return this;
}
SemiSecretFont* FlixelFont::fontWithSize(float s) { return (new FlixelFont)->initWithSize(s); }
// PORT: FlixelFont used a baked 8365-entry unichar->glyph table generated from the font's cmap;
// stb_truetype's cmap lookup gives the same mapping.
void FlixelFont::setGlyphs(uint16_t* glyphs, const unichar* chars, size_t length) {
    for (size_t i = 0; i < length; ++i)
        glyphs[i] = (chars[i] < 8365 && font) ? (uint16_t)stbtt_FindGlyphIndex(&font->info, chars[i]) : 0;
}

// ============================== SemiSecretText ==============================
static bool isNewline(char16_t c) { return (c >= 0x0A && c <= 0x0D) || c == 0x85 || c == 0x2028 || c == 0x2029; }
static bool isWhitespaceOrNewline(char16_t c) {
    return c == ' ' || c == '\t' || c == 0xA0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200A) || c == 0x202F || c == 0x205F ||
           c == 0x3000 || isNewline(c);
}
static const long NSNotFound_ = LONG_MAX;

// - (id) initWithFrame:(CGRect)frame
SemiSecretText* SemiSecretText::initWithFrame(CGRect f) {
    setFrame(f);
    wrapOnSpace = true;
    padding = 5.0f;
    reposition = true;
    color = CGColor{1, 1, 1, 1};
    shadowColor = CGColor{0, 0, 0, 1};
    text.clear();
    setFont(SemiSecretFont::fontWithName("Flixel", 8.0));
    effect = SemiSecretTextEffectNone;
    align = SemiSecretTextAlignCenter;
    lineSpacing = 1.6f;
    return this;
}

void SemiSecretText::setPadding(float p) {
    if (padding == p) return;
    padding = p;
    computeNewBounds();
}
void SemiSecretText::setText(const std::u16string& t) {
    text = t;
    computeNewBounds();
}
void SemiSecretText::setFont(SemiSecretFont* f) {
    if (!f) {
        useSSFont = false;
        ssfont = nullptr;
        computeNewBounds();
        return;
    }
    useSSFont = true;
    if (ssfont == f) return;
    ssfont = f;
    computeNewBounds();
}

// - (CGFloat) heightForLines:(NSInteger)lines givenWidth:(CGFloat)width
float SemiSecretText::heightForLines(int lines, float w) {
    float yy = 0.0f;
    long offset = 0;
    CGRect frameCopy = frame;
    setFrame(CGRectMake(frame.origin.x, frame.origin.y, w, frame.size.height));
    int currentLine = 0;
    while (offset != (long)text.size() && (currentLine < lines || lines == -1)) {
        long nextOffset = nextWrapOffsetForGlyphs(offset);
        offset = nextOffset;
        yy += textHeight * lineSpacing;
        currentLine++;
    }
    if (yy == 0) yy = textHeight * lineSpacing;
    setFrame(frameCopy);
    return yy + padding * 2;
}

// - (NSInteger) nextWrapOffsetForGlyphs:(NSInteger)offset
long SemiSecretText::nextWrapOffsetForGlyphs(long offset) {
    const long len = (long)text.size();
    const float unitsPerEm = ssfont && ssfont->font ? (float)ssfont->font->unitsPerEm : 1.0f;
    const float fsize = ssfont ? ssfont->size : 0;
    auto adv = [&](long i) -> float { return ssfont && ssfont->font ? glyphAdvance(ssfont->font, glyphs[i]) * fsize / unitsPerEm : 0; };
    long i;
    if (offset == len) {
        lineWidth = 0;
        return NSNotFound_;
    }
    if (!wrapped) {
        lineWidth = 0;
        for (i = offset; i < len; ++i) lineWidth += adv(i);
        return len;
    }
    long newline = NSNotFound_;
    for (i = offset; i < len; i++)
        if (isNewline(text[i])) { newline = i; break; }
    float widthAvail = bounds.size.width;
    float offsetWidth = 0.0f;
    long nextOffset = offset;
    for (nextOffset = offset; nextOffset < (long)glyphs.size(); ++nextOffset) {
        offsetWidth += adv(nextOffset);
        if (offsetWidth > widthAvail - padding * 2) break;
    }
    if (newline != NSNotFound_ && newline <= nextOffset) {
        nextOffset = newline + 1;
        lineWidth = 0;
        for (i = offset; i < nextOffset; ++i) lineWidth += adv(i);
        return nextOffset;
    }
    auto findFwd = [&](long from, bool (*pred)(char16_t), bool invert) -> long {
        for (long k = from; k < len; k++)
            if (pred(text[k]) != invert) return k;
        return NSNotFound_;
    };
    auto breakChar = wrapOnSpace ? isWhitespaceOrNewline : isNewline;
    if (nextOffset == len) {
        nextOffset = len;
    } else if (nextOffset == offset) {
        long ws = findFwd(nextOffset, breakChar, false);
        if (ws == NSNotFound_)
            nextOffset = len;
        else {
            long nonWs = findFwd(ws, isWhitespaceOrNewline, true);
            if (nonWs == NSNotFound_) nextOffset = len;
            else nextOffset = nonWs + 1;
        }
    } else {
        long ws = NSNotFound_;
        for (long k = nextOffset - 1; k >= offset; k--)
            if (breakChar(text[k])) { ws = k; break; }
        if (ws == nextOffset) {
            long nonWs = findFwd(ws, wrapOnSpace ? isWhitespaceOrNewline : isNewline, true);
            if (nonWs == NSNotFound_) nextOffset = len;
            else nextOffset = nonWs + 1;
        } else if (ws == NSNotFound_) {
            long ws2 = findFwd(nextOffset, breakChar, false);
            if (ws2 == NSNotFound_)
                nextOffset = len;
            else {
                long nonWs = findFwd(ws2, wrapOnSpace ? isWhitespaceOrNewline : isNewline, true);
                if (nonWs == NSNotFound_) nextOffset = len;
                else nextOffset = nonWs;
            }
        } else {
            nextOffset = ws + 1;
        }
    }
    lineWidth = 0;
    for (i = offset; i < nextOffset; ++i) lineWidth += adv(i);
    return nextOffset;
}

// - (void) computeNewBounds
void SemiSecretText::computeNewBounds() {
    if (useSSFont && ssfont) {
        glyphs.assign(text.size(), 0);
        ssfont->setGlyphs(glyphs.data(), (const unichar*)text.data(), text.size());
        int total = 0;
        if (ssfont->font)
            for (auto g : glyphs) total += glyphAdvance(ssfont->font, g);
        float upem = ssfont->font ? (float)ssfont->font->unitsPerEm : 1.0f;
        textWidth = total * ssfont->size / upem;
        textHeight = (ssfont->font ? ssfont->font->capHeight : 0) * ssfont->size / upem;
        CGRect previous_frame = frame;
        // UIView -setBounds: keeps the centre fixed
        float cx = frame.origin.x + frame.size.width / 2, cy = frame.origin.y + frame.size.height / 2;
        float w = textWidth + padding * 2, h = textHeight + padding * 2;
        setFrame(CGRectMake(cx - w / 2, cy - h / 2, w, h));
        if (reposition) {
            if (align == SemiSecretTextAlignLeft) frame.origin.x -= (frame.origin.x - previous_frame.origin.x);
            if (align == SemiSecretTextAlignRight) frame.origin.x += (frame.origin.x - previous_frame.origin.x);
        }
    }
}

// ---- glyph rasterisation (CGContextShowGlyphsAtPoint with kCGTextFill, y-down flipped context) ----
namespace {
struct Canvas {
    uint8_t* rgba;
    int w, h;
};

void drawGlyphMask(FontData* fd, float fsize, const uint16_t* g, long count, float px, float baseline, std::vector<float>& mask, int w,
                   int h) {
    float scale = fsize / fd->unitsPerEm;
    float pen = px;
    for (long i = 0; i < count; i++) {
        int ix = (int)std::floor(pen);
        int iy = (int)std::floor(baseline);
        float sx = pen - ix, sy = baseline - iy;
        int x0, y0, x1, y1;
        stbtt_GetGlyphBitmapBoxSubpixel(&fd->info, g[i], scale, scale, sx, sy, &x0, &y0, &x1, &y1);
        int gw = x1 - x0, gh = y1 - y0;
        if (gw > 0 && gh > 0) {
            std::vector<unsigned char> bm((size_t)gw * gh);
            stbtt_MakeGlyphBitmapSubpixel(&fd->info, bm.data(), gw, gh, gw, scale, scale, sx, sy, g[i]);
            for (int yy = 0; yy < gh; yy++) {
                int ty = iy + y0 + yy;
                if (ty < 0 || ty >= h) continue;
                for (int xx = 0; xx < gw; xx++) {
                    int tx = ix + x0 + xx;
                    if (tx < 0 || tx >= w) continue;
                    float c = bm[(size_t)yy * gw + xx] / 255.0f;
                    float& m = mask[(size_t)ty * w + tx];
                    m = m + c - m * c;  // union of overlapping glyph coverage
                }
            }
        }
        pen += glyphAdvance(fd, g[i]) * scale;
    }
}

void compositeMask(const std::vector<float>& mask, const CGColor& col, Canvas& cv) {
    for (int i = 0, n = cv.w * cv.h; i < n; i++) {
        float a = mask[i] * col.a;
        if (a <= 0) continue;
        uint8_t* d = &cv.rgba[(size_t)i * 4];
        float inv = 1 - a;
        d[0] = (uint8_t)std::lround(col.r * a * 255 + d[0] * inv);
        d[1] = (uint8_t)std::lround(col.g * a * 255 + d[1] * inv);
        d[2] = (uint8_t)std::lround(col.b * a * 255 + d[2] * inv);
        d[3] = (uint8_t)std::lround(a * 255 + d[3] * inv);
    }
}

// CGContextSetShadowWithColor(ctx, CGSizeZero, 5.0, color): gaussian of the drawn alpha.
// PORT: sigma = blur/2 approximates CoreGraphics' blur kernel.
std::vector<float> blurMask(const std::vector<float>& m, int w, int h, float blur) {
    float sigma = blur / 2;
    int r = (int)std::ceil(sigma * 3);
    std::vector<float> k(2 * r + 1);
    float sum = 0;
    for (int i = -r; i <= r; i++) sum += k[i + r] = std::exp(-(i * i) / (2 * sigma * sigma));
    for (auto& v : k) v /= sum;
    std::vector<float> tmp((size_t)w * h, 0), out((size_t)w * h, 0);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float s = 0;
            for (int i = -r; i <= r; i++) {
                int xx = x + i;
                if (xx >= 0 && xx < w) s += m[(size_t)y * w + xx] * k[i + r];
            }
            tmp[(size_t)y * w + x] = s;
        }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            float s = 0;
            for (int i = -r; i <= r; i++) {
                int yy = y + i;
                if (yy >= 0 && yy < h) s += tmp[(size_t)yy * w + x] * k[i + r];
            }
            out[(size_t)y * w + x] = s;
        }
    return out;
}
}  // namespace

// - (void) drawRect:(CGRect)rect
void SemiSecretText::renderInto(uint8_t* rgba, int pw, int ph) {
    Canvas cv{rgba, pw, ph};
    if (debug) {
        std::vector<float> full((size_t)pw * ph, 0);
        for (int y = 0; y < (int)bounds.size.height && y < ph; y++)
            for (int x = 0; x < (int)bounds.size.width && x < pw; x++) full[(size_t)y * pw + x] = 1;
        compositeMask(full, CGColor{1, 0, 0, 1}, cv);
    }
    if (!(useSSFont && ssfont && ssfont->font) || glyphs.empty()) return;
    FontData* fd = ssfont->font;
    const float fsize = ssfont->size;
    bool shadowOn = false;
    long offset = 0;
    float yy = 0.0f;
    const long len = (long)text.size();
    while (offset != len) {
        long nextOffset = nextWrapOffsetForGlyphs(offset);
        long notBlankOffset = nextOffset;
        if (notBlankOffset < len) {
            while (isWhitespaceOrNewline(text[notBlankOffset - 1]) && notBlankOffset > offset) notBlankOffset--;
        }
        float xx = padding;
        if (align == SemiSecretTextAlignCenter)
            xx = std::floor((frame.size.width - lineWidth) / 2);
        else if (align == SemiSecretTextAlignRight)
            xx = frame.size.width - lineWidth - padding;
        long count = notBlankOffset - offset;
        switch (effect) {
            case SemiSecretTextEffectEngravedShadow: {
                std::vector<float> m((size_t)pw * ph, 0);
                drawGlyphMask(fd, fsize, &glyphs[offset], count, xx, padding + textHeight - 1.0f + yy, m, pw, ph);
                compositeMask(m, shadowColor, cv);
                break;
            }
            case SemiSecretTextEffectReverseEngravedShadow: {
                std::vector<float> m((size_t)pw * ph, 0);
                drawGlyphMask(fd, fsize, &glyphs[offset], count, xx, padding + textHeight + 1.0f + yy, m, pw, ph);
                compositeMask(m, shadowColor, cv);
                break;
            }
            case SemiSecretTextEffectBlurredShadow:
                shadowOn = true;  // graphics state persists for the rest of drawRect
                break;
            default: break;
        }
        if (yy + textHeight * lineSpacing < bounds.size.height || !wrapped) {
            std::vector<float> m((size_t)pw * ph, 0);
            drawGlyphMask(fd, fsize, &glyphs[offset], count, xx, padding + textHeight + yy, m, pw, ph);
            if (shadowOn) compositeMask(blurMask(m, pw, ph, 5.0f), shadowColor, cv);
            compositeMask(m, color, cv);
        }
        offset = nextOffset;
        yy += textHeight * lineSpacing;
    }
}
