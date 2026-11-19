// ZigUI theme: palette (LAUNCHER_PLAN.md §4.1), fonts, ice-block frames, buttons, arrows,
// title-scene background and UI sounds. Two variants:
//   Real     - built at runtime from the user's imported ZiGGURAT assets (nothing bundled)
//   Fallback - procedural look-alike + SIL OFL pixel font (before a dump is imported)
#pragma once
#include <string>
#include <vector>

#include "launcher/ui/Font.h"
#include "launcher/ui/Gfx.h"

namespace zui {

namespace pal {
constexpr Color ink = Color::hex(0x000000);
constexpr Color night0 = Color::hex(0x11001F), night1 = Color::hex(0x1B0031), night2 = Color::hex(0x260043),
                night3 = Color::hex(0x2E0053), night4 = Color::hex(0x370060);
constexpr Color btnTop = Color::hex(0x1B0030), btnBottom = Color::hex(0x0E0018), btnBorder = Color::hex(0xD6D1DA);
constexpr Color select = Color::hex(0xFF0066);
constexpr Color pink = Color::hex(0xEA0069), eyePink = Color::hex(0xE50880);
constexpr Color ice = Color::hex(0x00F1FE), iceShade = Color::hex(0x008087), iceGlint = Color::hex(0xFFFFFF);
constexpr Color zigRed = Color::hex(0x870016), zigLight = Color::hex(0xFE002A), zigDark = Color::hex(0x45010C);
constexpr Color logoYellow = Color::hex(0xFEFF6F);
constexpr Color text = Color::hex(0xFFFFFF), textDim = Color::hex(0x999999), emphasis = Color::hex(0xCCFF00),
                ok = Color::hex(0x33FF66);
constexpr Color shadow = Color(0, 0, 0, 200);
}  // namespace pal

enum class FrameStyle { Small, Big };  // the two ice-block sizes in img_pillar.png

class Theme {
public:
    // assetsDir: imported game assets (may be missing); fallbackFont: OFL TTF path.
    void load(const std::string& assetsDir, const std::string& fallbackFont);
    void useFallback(bool on);     // force fallback (gallery / tests)
    bool real() const { return real_ && !forceFallback_; }
    bool realAvailable() const { return real_; }

    const Font& font() const;
    // Font sizes as used by the game's menus.
    static constexpr float kSmall = 8.0f, kLarge = 16.0f;

    // Widgets' look
    void frame(Rect r, FrameStyle s) const;          // ice-block frame + black inside
    Rect frameContent(Rect r, FrameStyle s) const;   // usable inner area
    void button(Rect r, bool focused, bool pressed) const;
    void chevron(int x, int y, bool right, double t, Color c = pal::pink) const;  // small bouncing chevron (8x9)
    void bigArrow(int x, int y, bool right, double t) const;   // arrowSheet frame (69x42) or procedural
    void background(double t) const;                // animated title-scene sky
    void logo(int cx, int y) const;                  // ziglogo / text fallback
    void silhouetteFlash(float alpha) const;         // PLAY flash (img_silhouette)
    void fit(const char* ctx, const std::string& s, float size, int maxW) const;
    void text(const std::string& s, int x, int y, float size, Color c = pal::text, Align a = Align::Left,
              int boxW = 0, bool shadowOn = true) const;

    // Sounds (no-ops in fallback theme: no game audio available)
    void sndMove() const;
    void sndConfirm() const;
    void sndError() const;
    std::string musicPath() const;

private:
    struct Block { Tex tex; int w = 0, h = 0; };
    struct BlockSet {
        std::vector<Block> h, v;   // horizontal row blocks, vertical column blocks
        int rowH = 0, colW = 0;    // strip thickness
        int gap = 1;               // gap between blocks
        int stick = 0;             // columns stick out of the rows by this many px (Big style)
    };
    BlockSet small_[2], big_[2];   // [0] fallback, [1] real
    const BlockSet& set(FrameStyle s) const;
    void buildReal(const std::string& dir);
    void buildFallback();
    std::vector<int> fill(const std::vector<Block>& blocks, int length, int gap, int& usedLen) const;

    Font realFont_, fallbackFont_;
    Tex btn_[2];
    Tex arrow_[2];
    std::vector<Tex> chev_[2];   // small chevron frames
    Tex sky_, stars_, moon_, cloud2_, cloud3_, cloud4_, mainCloud_, logo_, silhouette_;
    std::string dir_;
    bool real_ = false, forceFallback_ = false;
};

Theme& theme();

}  // namespace zui
