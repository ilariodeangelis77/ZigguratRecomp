// zigcore: controller -> touch mapping (LAUNCHER_PLAN.md §7.4).
//
// ZiGGURAT is a touch-only game. Instead of changing the game logic, every controller drives ONE
// virtual finger (gameplay) or a menu cursor whose button "taps" (menus). The decompiled Player
// code then sees exactly the touches it saw on the iPhone:
//   precision (scheme 0): any touch charges, aim angle = touchX / W * 270 + 135, release fires.
//   slingshot (scheme 1): touch must start within 50 px of (W/2,H/2); dragging > 20 px from the
//                         start charges; angle/power come from pivot -> finger.
// Pure and SDL-free so it can be unit-tested (ziginputtest) and reused by the launcher preview.
#pragma once
#include <cstdint>
#include <string>

#include "core/Config.h"

namespace zig {

enum PadButton : int {
    PB_A, PB_B, PB_X, PB_Y, PB_BACK, PB_GUIDE, PB_START, PB_LSTICK, PB_RSTICK, PB_LB, PB_RB,
    PB_DUP, PB_DDOWN, PB_DLEFT, PB_DRIGHT, PB_COUNT
};

// One frame of controller (+ keyboard) state. Axes -1..1 (y down), triggers 0..1, gyro rad/s
// (x = pitch, y = yaw, z = roll, SDL convention).
struct PadState {
    bool connected = false;
    float lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0;
    uint32_t buttons = 0;           // bit (1 << PadButton)
    float gyroX = 0, gyroY = 0, gyroZ = 0;
    // keyboard contributions (already resolved from KeyboardCfg by the caller)
    bool kbPress = false, kbRecenter = false;
    float kbX = 0, kbY = 0;         // -1..1
    bool down(PadButton b) const { return (buttons >> b) & 1u; }
};

enum class InputContext { Gameplay, Menu };

struct VirtualFinger { bool down = false; float x = 0, y = 0; };
struct MenuCursor { bool visible = false; float x = 240, y = 160; bool tap = false; };

// binding name -> pressed? ("rt"/"lt" use a 0.5 threshold). Unknown/empty = false.
bool bindingDown(const PadState& s, const std::string& binding);
int buttonFromName(const std::string& name);   // PadButton or -1 (lt/rt are not buttons)
const char* buttonName(int b);

// Radial deadzone + response curve: maps a stick vector to a 0..1-magnitude vector.
void applyDeadzone(float& x, float& y, float inner, float outer, const std::string& curve);

// Precision aim helpers (exactly the game's formula and its inverse).
float precisionAngleForX(float x, int W);           // x / W * 270 + 135
float precisionXForAngle(float angleDeg, int W);    // inverse, angle clamped to 135..405
// Stick direction (screen coords, y down) -> game aim angle in 135..405 (straight up = 270).
float aimAngleFromStick(float sx, float sy);

class InputMap {
public:
    // Constants of the original game (backing size 480x320).
    static constexpr float kPrecisionY = 290;    // where the virtual finger presses (bottom touch zone)
    static constexpr float kSlideSpeed = 240;    // px/s at full tilt, x sensitivity
    static constexpr float kCursorSpeed = 300;   // px/s, x cursor_speed
    static constexpr float kGyroPxPerRad = 300;  // gyro px per radian, x gyro sensitivity
    static constexpr float kStickEngage = 0.35f;   // "stick" aim mode: tilt past this to start charging
    static constexpr float kStickRelease = 0.20f;  // ...and let it fall below this to fire

    void reset(int W = 480, int H = 320);
    // scheme: PlayState ControlScheme (0 precision, 1 slingshot, 2 pivot-at-touch)
    void update(const PadState& s, const InputCfg& cfg, int scheme, InputContext ctx, float dt, int W = 480, int H = 320);

    const VirtualFinger& finger() const { return finger_; }
    const MenuCursor& cursor() const { return cursor_; }
    // True on frames where the controller/keyboard was actively used (stick past deadzone, button, gyro).
    bool activeThisFrame() const { return active_; }
    double idleSeconds() const { return idle_; }
    void hideCursor() { cursor_.visible = false; }
    void setCursor(float x, float y) { cursor_.x = x; cursor_.y = y; }
    float aimX() const { return aimX_; }       // precision finger X (persists between presses)
    float pullX() const { return pullX_; }     // slingshot displacement from the start point
    float pullY() const { return pullY_; }

private:
    VirtualFinger finger_;
    MenuCursor cursor_;
    bool active_ = false;
    double idle_ = 1e9;
    float aimX_ = 240;
    float pullX_ = 0, pullY_ = 0;
    float gyroAccX_ = 0, gyroAccY_ = 0;
    // aim_mode "stick": the shot follows the stick direction (both schemes); tilt = charge, release = fire
    bool stickHeld_ = false, stickFresh_ = false;
    float stickDirX_ = 0, stickDirY_ = -1, stickPeak_ = 0;
    void updateStick(float sx, float sy);
    // charge state for aim_mode "stick": starts on a fresh tilt OR a fresh press, fires (ends) as soon
    // as EITHER the stick springs back or the button is released -- whichever comes first.
    bool charging_ = false, prevStickOn_ = false, prevBtnOn_ = false;
    bool stickCharge(bool stickOn, bool btnOn);
    bool wasPress_ = false, wasRecenter_ = false;
    InputContext lastCtx_ = InputContext::Menu;
    bool ctxInit_ = false;
};

}  // namespace zig
