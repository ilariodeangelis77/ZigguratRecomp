// ZigUI — tiny immediate-mode pixel UI for the ZiGGURAT launcher (LAUNCHER_PLAN.md §4, §6).
//
// Usage (one frame):
//   zui::UI& ui = zui::ui();
//   ui.beginFrame(now);                 // after feeding SDL events to zui::input()
//   theme().background(now);
//   ui.panel({10,40,460,200});
//   if (ui.toggle("vsync", row, "V-SYNC", cfg.vsync, "Sync to the display")) changed = true;
//   if (ui.bigButton("play", r, "PLAY NOW")) startGame();
//   ui.helpLine({8,290,464,12});
//   ui.prompts({8,304,300,12}, {{Act::Ok,"SELECT"},{Act::Back,"BACK"}});
//   ui.endFrame();                      // resolves navigation for the next frame
//
// Navigation: focus moves spatially between focusable widgets registered this frame
// (D-pad/stick/arrows/WASD/Tab). Selector/Slider/Toggle consume Left/Right; List consumes Up/Down.
// Mouse hover focuses, click activates; wheel scrolls lists. LB/RB (Q/E) drive tabBar().
// Widgets return true when their value changed or they were activated.
// IDs are strings unique per screen; the focused id survives between frames.
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "launcher/ui/Input.h"
#include "launcher/ui/Theme.h"

namespace zui {

struct Prompt { Act act; std::string label; };

class UI {
public:
    void beginFrame(double now);
    void endFrame();

    // --- static drawing
    void panel(Rect r, FrameStyle s = FrameStyle::Small);
    void label(const std::string& text, int x, int y, float size = Theme::kSmall, Color c = pal::text,
               Align a = Align::Left, int boxW = 0);
    int textBlock(Rect r, const std::string& text, float size = Theme::kSmall, Color c = pal::text,
                  Align a = Align::Left);   // wrapped; returns used height
    void progress(Rect r, float frac, const std::string& caption = "");
    void helpLine(Rect r);                   // one-line help of the focused widget
    void prompts(Rect r, const std::vector<Prompt>& items);  // device-aware button prompts

    // --- focusable widgets (row widgets use the whole row; focused row gets the pink bar)
    bool button(const std::string& id, Rect r, const std::string& text, const std::string& help = "");
    bool bigButton(const std::string& id, Rect r, const std::string& text, const std::string& help = "");
    bool toggle(const std::string& id, Rect row, const std::string& label, bool& v, const std::string& help = "",
                const std::string& onText = "ON", const std::string& offText = "OFF");
    bool selector(const std::string& id, Rect row, const std::string& label, const std::vector<std::string>& opts,
                  int& idx, const std::string& help = "");
    bool slider(const std::string& id, Rect row, const std::string& label, float& v, float lo, float hi, float step,
                const std::string& help = "", const std::string& valueText = "");
    bool tabBar(const std::string& id, Rect r, const std::vector<std::string>& tabs, int& idx);
    // Scrollable list; returns true when an item is activated (Ok/click). sel = highlighted item.
    bool list(const std::string& id, Rect r, const std::vector<std::string>& items, int& sel,
              const std::string& help = "", int rowH = 14);

    // --- modal dialog: call every frame while open; returns pressed button index or -1.
    int messageBox(const std::string& id, const std::string& title, const std::string& text,
                   const std::vector<std::string>& buttons);

    // --- focus control
    void focus(const std::string& id);
    const std::string& focused() const { return focus_; }
    bool isFocused(const std::string& id) const { return focus_ == id; }
    void error();                            // error sound + small shake on the focused row
    double now() const { return now_; }
    // Back button pressed this frame and not consumed by a modal.
    bool back() const;
    // Called when focus changes or a widget activates (sound hooks are built in).
    std::function<void()> onActivate;

private:
    struct Item { std::string id; Rect r; std::string help; bool modal; };
    std::vector<Item> items_;
    std::string focus_, help_, prevFocus_;
    std::string modalId_;
    bool modalThisFrame_ = false, modalActive_ = false, inModal_ = false;
    std::string preModalFocus_;
    double now_ = 0, shakeUntil_ = 0;
    bool consumedLR_ = false, consumedUD_ = false;
    std::string pointerDownId_;

    bool interactive() const { return !modalActive_ || inModal_; }
    void reg(const std::string& id, Rect r, const std::string& help);
    bool hovered(Rect r) const;
    bool clicked(const std::string& id, Rect r);
    bool activated(const std::string& id, Rect r);
    void rowBar(const std::string& id, Rect row);
    int shakeX(const std::string& id) const;
};

UI& ui();

}  // namespace zui
