#include "launcher/ui/ZigUI.h"
#include "core/TestHooks.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace zui {

UI& ui() {
    static UI u;
    return u;
}

void UI::beginFrame(double now) {
    now_ = now;
    items_.clear();
    help_.clear();
    consumedLR_ = consumedUD_ = false;
    modalThisFrame_ = false;
    inModal_ = false;
}

void UI::reg(const std::string& id, Rect r, const std::string& help) {
    if (modalActive_ && !inModal_) return;  // inert behind a modal
    items_.push_back({id, r, help, inModal_});
    if (focus_ == id) help_ = help;
    // mouse hover focuses
    Input& in = input();
    if (in.pointerMoved && in.usingPointer && r.contains(in.px, in.py) && focus_ != id) {
        focus_ = id;
        help_ = help;
        theme().sndMove();
    }
}

bool UI::hovered(Rect r) const { return input().usingPointer && r.contains(input().px, input().py); }

bool UI::clicked(const std::string& id, Rect r) {
    Input& in = input();
    if (!interactive()) return false;
    if (in.pointerPressed && r.contains(in.px, in.py)) pointerDownId_ = id;
    if (in.pointerReleased && r.contains(in.px, in.py) && pointerDownId_ == id) {
        pointerDownId_.clear();
        return true;
    }
    return false;
}

bool UI::activated(const std::string& id, Rect r) {
    bool a = clicked(id, r) || (interactive() && focus_ == id && input().hit(Act::Ok));
    if (a) {
        theme().sndConfirm();
        if (onActivate) onActivate();
    }
    return a;
}

void UI::focus(const std::string& id) { focus_ = id; }

void UI::error() {
    theme().sndError();
    shakeUntil_ = now_ + 0.25;
}

int UI::shakeX(const std::string& id) const {
    if (focus_ != id || now_ > shakeUntil_) return 0;
    return (int)std::lround(std::sin(now_ * 80) * 2);
}

bool UI::back() const { return !modalActive_ && input().hit(Act::Back); }

void UI::rowBar(const std::string& id, Rect row) {
    if (focus_ == id) gfx().fill(row, pal::select);  // the game's pink selection bar (OptionsState 0xFF0066)
}

// ------------------------------------------------------------------ static drawing
void UI::panel(Rect r, FrameStyle s) { theme().frame(r, s); }

void UI::label(const std::string& t, int x, int y, float size, Color c, Align a, int boxW) {
    theme().text(t, x, y, size, c, a, boxW);
}

int UI::textBlock(Rect r, const std::string& t, float size, Color c, Align a) {
    const Font& f = theme().font();
    int lh = f.lineHeight(size) + 1, y = r.y;
    for (auto& line : f.wrap(t, size, r.w)) {
        if (y + lh > r.y + r.h + lh / 2) {
            theme().fit("textblock-cut", t, size, 0);  // text did not fit vertically
            break;
        }
        theme().text(line, r.x, y, size, c, a, r.w);
        y += lh;
    }
    return y - r.y;
}

void UI::progress(Rect r, float frac, const std::string& caption) {
    frac = std::clamp(frac, 0.0f, 1.0f);
    gfx().fill(r, pal::ink);
    gfx().fill({r.x, r.y, r.w, 1}, pal::iceShade);
    gfx().fill({r.x, r.y + r.h - 1, r.w, 1}, pal::iceShade);
    gfx().fill({r.x, r.y, 1, r.h}, pal::iceShade);
    gfx().fill({r.x + r.w - 1, r.y, 1, r.h}, pal::iceShade);
    // ice-block fill: 6px cells
    int inner = r.w - 4, cells = std::max(1, inner / 7), lit = (int)std::lround(cells * frac);
    for (int i = 0; i < cells; i++) {
        Rect c{r.x + 2 + i * 7, r.y + 2, 6, r.h - 4};
        gfx().fill(c, i < lit ? pal::ice : pal::night1);
        if (i < lit) gfx().fill({c.x, c.y, c.w, std::max(1, c.h / 3)}, pal::iceGlint);
    }
    if (!caption.empty()) theme().text(caption, r.x, r.y - 11, Theme::kSmall, pal::text, Align::Center, r.w);
}

void UI::helpLine(Rect r) {
    if (help_.empty()) return;
    theme().text(help_, r.x, r.y, Theme::kSmall, pal::textDim, Align::Left, r.w);
}

namespace {
void glyphCircle(int x, int y, Color fill, Color border) {
    static const int w[11] = {3, 7, 9, 9, 11, 11, 11, 9, 9, 7, 3};
    for (int j = 0; j < 11; j++) gfx().fill({x + (11 - w[j]) / 2, y + j, w[j], 1}, border);
    for (int j = 1; j < 10; j++) {
        int ww = std::max(0, w[j] - 2);
        gfx().fill({x + (11 - ww) / 2, y + j, ww, 1}, fill);
    }
}
// PlayStation-style symbols drawn as pixel art (generic shapes, not logos).
void psSymbol(int x, int y, Act a) {
    Color c;
    switch (a) {
        case Act::Ok:  // cross
            c = Color::hex(0x7FB2FF);
            for (int i = 0; i < 5; i++) { gfx().fill({x + 3 + i, y + 3 + i, 1, 1}, c); gfx().fill({x + 7 - i, y + 3 + i, 1, 1}, c); }
            break;
        case Act::Back:  // circle
            c = Color::hex(0xFF6B6B);
            gfx().fill({x + 4, y + 2, 3, 1}, c); gfx().fill({x + 4, y + 8, 3, 1}, c);
            gfx().fill({x + 2, y + 4, 1, 3}, c); gfx().fill({x + 8, y + 4, 1, 3}, c);
            gfx().fill({x + 3, y + 3, 1, 1}, c); gfx().fill({x + 7, y + 3, 1, 1}, c);
            gfx().fill({x + 3, y + 7, 1, 1}, c); gfx().fill({x + 7, y + 7, 1, 1}, c);
            break;
        case Act::X:  // square
            c = Color::hex(0xFF8AD8);
            gfx().fill({x + 3, y + 3, 5, 1}, c); gfx().fill({x + 3, y + 7, 5, 1}, c);
            gfx().fill({x + 3, y + 3, 1, 5}, c); gfx().fill({x + 7, y + 3, 1, 5}, c);
            break;
        default:  // triangle
            c = Color::hex(0x5FE3B0);
            for (int j = 0; j < 5; j++) { gfx().fill({x + 5 - j, y + 3 + j, 1, 1}, c); gfx().fill({x + 5 + j, y + 3 + j, 1, 1}, c); }
            gfx().fill({x + 1, y + 7, 9, 1}, c);
            break;
    }
}
}  // namespace

void UI::prompts(Rect r, const std::vector<Prompt>& items) {
    Input& in = input();
    const Font& f = theme().font();
    // dark footer band so prompts stay readable over the bright cloud layer
    gfx().fill({0, r.y - 4, kW, kH - (r.y - 4)}, Color(0, 0, 0, 170));
    int x = r.x;
    for (auto& p : items) {
        int gw = 11;
        if (in.device == Device::Keyboard) {
            const char* k = "?";
            switch (p.act) {
                case Act::Ok: k = "ENTER"; break;
                case Act::Back: k = "ESC"; break;
                case Act::TabPrev: k = "Q"; break;
                case Act::TabNext: k = "E"; break;
                case Act::Start: k = "F5"; break;
                case Act::Y: k = "R"; break;
                case Act::X: k = "X"; break;
                default: k = "ARROWS"; break;
            }
            int tw = f.width(k, Theme::kSmall);
            gw = tw + 6;
            gfx().fill({x, r.y, gw, 11}, pal::btnBorder);
            gfx().fill({x + 1, r.y + 1, gw - 2, 9}, pal::night1);
            theme().text(k, x + 3, r.y + 1, Theme::kSmall, pal::text, Align::Left, 0, false);
        } else if (in.device == Device::PlayStation && p.act <= Act::Y && (p.act == Act::Ok || p.act == Act::Back || p.act == Act::X || p.act == Act::Y)) {
            glyphCircle(x, r.y, pal::night1, pal::textDim);
            psSymbol(x, r.y, p.act);
        } else {
            const char* letter = "";
            Color fill = pal::night1;
            bool nin = in.device == Device::Nintendo;
            switch (p.act) {
                case Act::Ok: letter = nin ? "B" : "A"; fill = Color::hex(0x2E8B3A); break;
                case Act::Back: letter = nin ? "A" : "B"; fill = Color::hex(0xB3263A); break;
                case Act::X: letter = nin ? "Y" : "X"; fill = Color::hex(0x2D5BB8); break;
                case Act::Y: letter = nin ? "X" : "Y"; fill = Color::hex(0xB89A1E); break;
                case Act::TabPrev: letter = in.device == Device::PlayStation ? "L1" : nin ? "L" : "LB"; break;
                case Act::TabNext: letter = in.device == Device::PlayStation ? "R1" : nin ? "R" : "RB"; break;
                case Act::Start: letter = in.device == Device::PlayStation ? "OPT" : nin ? "+" : "START"; break;
                default: letter = "+"; break;
            }
            if (nin) fill = pal::night1;
            int tw = f.width(letter, Theme::kSmall);
            if (strlen(letter) <= 1) {
                glyphCircle(x, r.y, fill, pal::textDim);
                theme().text(letter, x + (11 - tw) / 2 + 1, r.y + 1, Theme::kSmall, pal::text, Align::Left, 0, false);
            } else {
                gw = tw + 6;
                gfx().fill({x + 1, r.y, gw - 2, 11}, pal::textDim);
                gfx().fill({x, r.y + 1, gw, 9}, pal::textDim);
                gfx().fill({x + 1, r.y + 1, gw - 2, 9}, pal::night1);
                theme().text(letter, x + 3, r.y + 1, Theme::kSmall, pal::text, Align::Left, 0, false);
            }
        }
        x += gw + 3;
        theme().text(p.label, x, r.y + 1, Theme::kSmall, pal::text);
        x += f.width(p.label, Theme::kSmall) + 10;
        if (x > r.x + r.w) break;
    }
}

// ------------------------------------------------------------------ focusable widgets
bool UI::button(const std::string& id, Rect r, const std::string& text, const std::string& help) {
    reg(id, r, help);
    bool down = input().pointerDown && pointerDownId_ == id && hovered(r);
    Rect rr = r;
    rr.x += shakeX(id);
    theme().button(rr, focus_ == id, down);
    const Font& f = theme().font();
    int th = f.lineHeight(Theme::kSmall);
    theme().text(text, rr.x, rr.y + (r.h - th) / 2 + (down ? 1 : 0), Theme::kSmall,
                 focus_ == id ? pal::text : pal::btnBorder, Align::Center, r.w);
    return activated(id, r);
}

bool UI::bigButton(const std::string& id, Rect r, const std::string& text, const std::string& help) {
    reg(id, r, help);
    bool foc = focus_ == id;
    bool down = input().pointerDown && pointerDownId_ == id && hovered(r);
    Rect rr = r;
    rr.x += shakeX(id);
    theme().button(rr, foc, down);
    // "GIOCA ORA" style: large text, pink when focused, chevrons bounce towards it.
    const Font& f = theme().font();
    int th = f.lineHeight(Theme::kLarge);
    theme().text(text, rr.x, rr.y + (r.h - th) / 2 + (down ? 1 : 0), Theme::kLarge, foc ? pal::select : pal::text,
                 Align::Center, r.w);
    if (foc) {
        theme().chevron(rr.x - 27, rr.y + (r.h - 14) / 2, true, now_);
        theme().chevron(rr.x + r.w + 4, rr.y + (r.h - 14) / 2, false, now_);
    }
    return activated(id, r);
}

bool UI::toggle(const std::string& id, Rect row, const std::string& lab, bool& v, const std::string& help,
                const std::string& onText, const std::string& offText) {
    reg(id, row, help);
    rowBar(id, row);
    int sx = shakeX(id);
    theme().text(lab, row.x + 4 + sx, row.y + (row.h - 9) / 2, Theme::kSmall);
    const std::string& vt = v ? onText : offText;
    theme().fit("toggle-label", lab, Theme::kSmall,
                row.w - 16 - std::max(theme().font().width(onText, Theme::kSmall), theme().font().width(offText, Theme::kSmall)));
    Color vc = v ? (focus_ == id ? pal::text : pal::select) : pal::textDim;
    theme().text(vt, row.x + sx, row.y + (row.h - 9) / 2, Theme::kSmall, vc, Align::Right, row.w - 8);
    bool changed = false;
    if (interactive() && focus_ == id && (input().hit(Act::Left) || input().hit(Act::Right))) {
        consumedLR_ = true;
        changed = true;
        theme().sndMove();
    }
    if (activated(id, row)) changed = true;
    if (changed) v = !v;
    return changed;
}

bool UI::selector(const std::string& id, Rect row, const std::string& lab, const std::vector<std::string>& opts,
                  int& idx, const std::string& help) {
    reg(id, row, help);
    rowBar(id, row);
    if (opts.empty()) return false;
    idx = std::clamp(idx, 0, (int)opts.size() - 1);
    int sx = shakeX(id);
    int ty = row.y + (row.h - 9) / 2;
    theme().text(lab, row.x + 4 + sx, ty, Theme::kSmall);
    // value area: right half with bouncing chevrons on both sides
    int vw = std::max(110, row.w / 2 - 10);
    Rect val{row.x + row.w - vw - 4, row.y, vw, row.h};
    theme().fit("selector-label", lab, Theme::kSmall, val.x - row.x - 8);
    for (auto& o : opts) theme().fit("selector-value", o, Theme::kSmall, val.w - 48);
    Rect leftHit{val.x, row.y, 26, row.h}, rightHit{val.x + val.w - 26, row.y, 26, row.h};
    bool foc = focus_ == id;
    if (foc) {
        theme().chevron(val.x + sx, row.y + (row.h - 14) / 2, false, now_);
        theme().chevron(val.x + val.w - 23 + sx, row.y + (row.h - 14) / 2, true, now_);
    } else {
        theme().text("<", val.x + 4, ty, Theme::kSmall, pal::textDim);
        theme().text(">", val.x + val.w - 10, ty, Theme::kSmall, pal::textDim);
    }
    theme().text(opts[idx], val.x + sx, ty, Theme::kSmall, foc ? pal::text : pal::emphasis, Align::Center, val.w);
    int d = 0;
    if (interactive() && foc && input().hit(Act::Left)) d = -1;
    if (interactive() && foc && input().hit(Act::Right)) d = +1;
    if (d) consumedLR_ = true;
    if (clicked(id + "#l", leftHit)) d = -1;
    else if (clicked(id + "#r", rightHit)) d = +1;
    else if (clicked(id, row) || (interactive() && foc && input().hit(Act::Ok))) d = +1;
    if (!d) return false;
    int n = (int)opts.size();
    idx = ((idx + d) % n + n) % n;
    theme().sndMove();
    return true;
}

bool UI::slider(const std::string& id, Rect row, const std::string& lab, float& v, float lo, float hi, float step,
                const std::string& help, const std::string& valueText) {
    reg(id, row, help);
    rowBar(id, row);
    int sx = shakeX(id);
    int ty = row.y + (row.h - 9) / 2;
    theme().text(lab, row.x + 4 + sx, ty, Theme::kSmall);
    // row of small ice blocks
    const int cells = 10, cw = 7, gap = 2;
    int bw = cells * (cw + gap) - gap;
    char buf[32];
    std::string vt = valueText;
    if (vt.empty()) { snprintf(buf, sizeof buf, "%d%%", (int)std::lround((v - lo) / (hi - lo) * 100)); vt = buf; }
    int tw = 30;
    Rect bar{row.x + row.w - 4 - tw - 6 - bw + sx, row.y + (row.h - 8) / 2, bw, 8};
    theme().fit("slider-label", lab, Theme::kSmall, bar.x - row.x - 8);
    float frac = (hi > lo) ? (v - lo) / (hi - lo) : 0;
    int lit = (int)std::lround(frac * cells);
    for (int i = 0; i < cells; i++) {
        Rect c{bar.x + i * (cw + gap), bar.y, cw, 8};
        gfx().fill({c.x - 1, c.y - 1, c.w + 2, c.h + 2}, pal::ink);
        gfx().fill(c, i < lit ? pal::ice : pal::night2);
        if (i < lit) {
            gfx().fill({c.x + 1, c.y + 1, c.w - 2, 2}, pal::iceGlint);
            gfx().fill({c.x, c.y + c.h - 2, c.w, 2}, pal::iceShade);
        }
    }
    theme().text(vt, row.x + sx, ty, Theme::kSmall, focus_ == id ? pal::text : pal::emphasis, Align::Right, row.w - 4);
    float old = v;
    if (interactive() && focus_ == id) {
        if (input().hit(Act::Left)) { v -= step; consumedLR_ = true; }
        if (input().hit(Act::Right)) { v += step; consumedLR_ = true; }
    }
    Input& in = input();
    if (interactive() && in.pointerDown && in.usingPointer && Rect{bar.x - 4, row.y, bar.w + 8, row.h}.contains(in.px, in.py)) {
        float f = (in.px - bar.x) / (float)bar.w;
        v = lo + std::clamp(f, 0.0f, 1.0f) * (hi - lo);
        if (step > 0) v = lo + std::round((v - lo) / step) * step;
    }
    v = std::clamp(v, lo, hi);
    if (v != old) { theme().sndMove(); return true; }
    return false;
}

bool UI::tabBar(const std::string& id, Rect r, const std::vector<std::string>& tabs, int& idx) {
    if (tabs.empty()) return false;
    int n = (int)tabs.size(), old = idx;
    const Font& f = theme().font();
    int x = r.x;
    for (int i = 0; i < n; i++) {
        int w = f.width(tabs[i], Theme::kSmall) + 14;
        Rect t{x, r.y, w, r.h};
        std::string tid = id + "#" + std::to_string(i);
        reg(tid, t, "");
        bool sel = i == idx;
        // selected tab: ice-cyan tab with black text; others dark purple
        gfx().fill(t, pal::ink);
        gfx().fill(t.inset(1), sel ? pal::ice : pal::night1);
        if (sel) gfx().fill({t.x + 1, t.y + 1, t.w - 2, 2}, pal::iceGlint);
        if (focus_ == tid) {
            gfx().fill({t.x, t.y + t.h - 2, t.w, 2}, pal::select);
        }
        theme().text(tabs[i], t.x, t.y + (t.h - 9) / 2, Theme::kSmall, sel ? pal::ink : pal::text, Align::Center, t.w,
                     !sel);
        if (clicked(tid, t) || (interactive() && focus_ == tid && input().hit(Act::Ok))) idx = i;
        x += w + 2;
    }
    {
        std::string all;
        for (auto& t : tabs) all += t + " ";
        if (x - 2 > r.x + r.w) theme().fit("tabbar", all, Theme::kSmall, 0);
    }
    if (interactive() && !modalActive_) {
        if (input().hit(Act::TabPrev)) idx = (idx + n - 1) % n;
        if (input().hit(Act::TabNext)) idx = (idx + 1) % n;
    }
    if (idx != old) { theme().sndMove(); return true; }
    return false;
}

bool UI::list(const std::string& id, Rect r, const std::vector<std::string>& items, int& sel, const std::string& help,
              int rowH) {
    reg(id, r, help);
    struct Scroll { std::string id; int top = 0; };
    static std::vector<Scroll> scrolls;
    Scroll* sc = nullptr;
    for (auto& s : scrolls) if (s.id == id) sc = &s;
    if (!sc) { scrolls.push_back({id, 0}); sc = &scrolls.back(); }
    int n = (int)items.size(), visible = std::max(1, r.h / rowH);
    sel = std::clamp(sel, 0, std::max(0, n - 1));
    bool foc = focus_ == id, act = false;
    if (interactive() && foc && n > 0) {
        if (input().hit(Act::Up) && sel > 0) { sel--; consumedUD_ = true; theme().sndMove(); }
        if (input().hit(Act::Down) && sel < n - 1) { sel++; consumedUD_ = true; theme().sndMove(); }
        if (input().hit(Act::Ok)) { act = true; theme().sndConfirm(); }
    }
    if (interactive() && hovered(r) && input().wheel != 0) sc->top -= (int)input().wheel;
    if (sel < sc->top) sc->top = sel;
    if (sel >= sc->top + visible) sc->top = sel - visible + 1;
    sc->top = std::clamp(sc->top, 0, std::max(0, n - visible));
    gfx().setClip(&r);
    for (int i = 0; i < visible && sc->top + i < n; i++) {
        int k = sc->top + i;
        Rect row{r.x, r.y + i * rowH, r.w - 6, rowH};
        if (k == sel) gfx().fill(row, foc ? pal::select : pal::night3);
        theme().fit("list-item", items[k], Theme::kSmall, row.w - 8);
        theme().text(items[k], row.x + 4, row.y + (rowH - 9) / 2, Theme::kSmall, k == sel || foc ? pal::text : pal::btnBorder);
        Input& in = input();
        if (interactive() && in.usingPointer && in.pointerReleased && row.contains(in.px, in.py)) {
            if (sel == k) { act = true; theme().sndConfirm(); }
            sel = k;
            focus_ = id;
        }
    }
    gfx().setClip(nullptr);
    // scrollbar
    if (n > visible) {
        int th = std::max(6, r.h * visible / n), ty = r.y + (r.h - th) * sc->top / std::max(1, n - visible);
        gfx().fill({r.x + r.w - 3, r.y, 3, r.h}, pal::night1);
        gfx().fill({r.x + r.w - 3, ty, 3, th}, pal::ice);
    }
    return act;
}

int UI::messageBox(const std::string& id, const std::string& title, const std::string& text,
                   const std::vector<std::string>& buttons) {
    modalThisFrame_ = true;
    bool openedNow = false;
    if (!modalActive_ || modalId_ != id) {
        // first frame: take focus
        preModalFocus_ = focus_;
        modalId_ = id;
        modalActive_ = true;
        focus_ = id + "#b0";
        openedNow = true;
    }
    inModal_ = true;
    gfx().fill({0, 0, kW, kH}, Color(0, 0, 0, 160));
    const Font& f = theme().font();
    auto lines = f.wrap(text, Theme::kSmall, 300);
    int lh = f.lineHeight(Theme::kSmall) + 1;
    int h = 26 + 18 + (int)lines.size() * lh + 34 + 20;
    Rect box{kW / 2 - 170, kH / 2 - h / 2, 340, h};
    theme().frame(box, FrameStyle::Small);
    Rect c = theme().frameContent(box, FrameStyle::Small);
    theme().text(title, c.x, c.y + 2, Theme::kLarge, pal::text, Align::Center, c.w);
    int y = c.y + 22;
    for (auto& l : lines) { theme().text(l, c.x, y, Theme::kSmall, pal::btnBorder, Align::Center, c.w); y += lh; }
    int nb = (int)buttons.size(), bw = 96, total = nb * bw + (nb - 1) * 10, bx = c.x + (c.w - total) / 2;
    int result = -1;
    for (int i = 0; i < nb; i++) {
        if (button(id + "#b" + std::to_string(i), {bx + i * (bw + 10), c.y + c.h - 26, bw, 22}, buttons[i])) result = i;
    }
    if (input().hit(Act::Back) && nb > 0) result = nb - 1;  // Back = last button (cancel)
    // The press that opened the dialog must not also answer it (e.g. ADD TO STEAM -> "Close Steam?"):
    // ignore every input on the dialog's first frame.
    if (openedNow) result = -1;
    inModal_ = false;
    if (result >= 0) {
        modalActive_ = false;
        modalId_.clear();
        focus_ = preModalFocus_;
    }
    return result;
}

// ------------------------------------------------------------------ navigation
void UI::endFrame() {
    if (!modalThisFrame_ && modalActive_) {  // modal vanished without a result
        modalActive_ = false;
        modalId_.clear();
        focus_ = preModalFocus_;
    }
    if (items_.empty()) return;
    const Item* cur = nullptr;
    for (auto& it : items_) if (it.id == focus_) cur = &it;
    if (!cur) {  // focus lost: first focusable
        focus_ = items_.front().id;
        return;
    }
    Input& in = input();
    int dx = 0, dy = 0;
    if (in.hit(Act::Up) && !consumedUD_) dy = -1;
    if (in.hit(Act::Down) && !consumedUD_) dy = 1;
    if (in.hit(Act::Left) && !consumedLR_) dx = -1;
    if (in.hit(Act::Right) && !consumedLR_) dx = 1;
    if (!dx && !dy) return;
    if (zig::testEnv("ZUI_DEBUG")) SDL_Log("nav dx=%d dy=%d focus=%s items=%d", dx, dy, focus_.c_str(), (int)items_.size());
    const Item* best = nullptr;
    long long bestScore = 1LL << 40;
    for (auto& it : items_) {
        if (&it == cur) continue;
        // gap between the two rects on an axis (0 when their ranges overlap)
        auto gapX = [&] { return std::max(0, std::max(it.r.x - (cur->r.x + cur->r.w), cur->r.x - (it.r.x + it.r.w))); };
        auto gapY = [&] { return std::max(0, std::max(it.r.y - (cur->r.y + cur->r.h), cur->r.y - (it.r.y + it.r.h))); };
        long long along, across;
        if (dy) {
            // must be beyond the current row edge in the movement direction
            if (dy > 0 ? it.r.y < cur->r.y + cur->r.h - 2 : it.r.y + it.r.h > cur->r.y + 2) continue;
            along = gapY();
            across = gapX();
        } else {
            if (dx > 0 ? it.r.x < cur->r.x + cur->r.w - 2 : it.r.x + it.r.w > cur->r.x + 2) continue;
            along = gapX();
            across = gapY();
        }
        // prefer overlapping neighbours; tie-break on centre distance
        long long tie = std::abs(it.r.cx() - cur->r.cx()) + std::abs(it.r.cy() - cur->r.cy());
        long long score = (along + across * 4) * 1000 + tie;
        if (score < bestScore) { bestScore = score; best = &it; }
    }
    if (best) {
        focus_ = best->id;
        theme().sndMove();
    }
}

}  // namespace zui
