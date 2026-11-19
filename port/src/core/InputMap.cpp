#include "core/InputMap.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace zig {
namespace {
const char* kNames[PB_COUNT] = {"a", "b", "x", "y", "back", "guide", "start", "lstick", "rstick", "lb", "rb",
                                "dpad_up", "dpad_down", "dpad_left", "dpad_right"};
float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
}  // namespace

int buttonFromName(const std::string& n) {
    for (int i = 0; i < PB_COUNT; i++)
        if (n == kNames[i]) return i;
    return -1;
}
const char* buttonName(int b) { return (b >= 0 && b < PB_COUNT) ? kNames[b] : ""; }

bool bindingDown(const PadState& s, const std::string& b) {
    if (b.empty()) return false;
    if (b == "rt") return s.rt > 0.5f;
    if (b == "lt") return s.lt > 0.5f;
    int i = buttonFromName(b);
    return i >= 0 && s.down((PadButton)i);
}

void applyDeadzone(float& x, float& y, float inner, float outer, const std::string& curve) {
    float m = std::sqrt(x * x + y * y);
    if (outer <= inner + 0.01f) outer = inner + 0.01f;
    if (m <= inner || m <= 0) { x = y = 0; return; }
    float t = clampf((m - inner) / (outer - inner), 0, 1);
    if (curve == "quadratic") t = t * t;
    x = x / m * t;
    y = y / m * t;
}

float precisionAngleForX(float x, int W) { return x / (float)W * 270.0f + 135.0f; }

float precisionXForAngle(float a, int W) {
    a = clampf(a, 135.0f, 405.0f);
    return (a - 135.0f) / 270.0f * (float)W;
}

float aimAngleFromStick(float sx, float sy) {
    // screen coords (y down): atan2(y, x) in degrees; straight up = 270, matching cos/sin in Player.
    float a = std::atan2(sy, sx) * 57.29577951308232f;
    if (a < 0) a += 360.0f;          // 0..360
    if (a < 45.0f) a += 360.0f;      // 0..45 (right/down-right) -> 360..405
    // a is now in 45..405; the downward wedge 45..135 is not aimable: clamp to the nearer edge.
    if (a < 135.0f) a = (a < 90.0f) ? 405.0f : 135.0f;
    return a;
}

// "stick" aim mode: latch the stick direction while tilted, remember the peak tilt, and report a
// release when the stick springs back (hysteresis between kStickEngage and kStickRelease).
void InputMap::updateStick(float sx, float sy) {
    float m = std::sqrt(sx * sx + sy * sy);
    stickFresh_ = false;
    if (!stickHeld_) {
        if (m > kStickEngage) { stickHeld_ = true; stickFresh_ = true; stickPeak_ = 0; }
    } else if (m < kStickRelease) {
        stickHeld_ = false;
    }
    if (stickHeld_ && m > kStickRelease) {
        stickDirX_ = sx / m;
        stickDirY_ = sy / m;
        stickPeak_ = std::max(stickPeak_, std::min(m, 1.0f));
    }
}

bool InputMap::stickCharge(bool stickOn, bool btnOn) {
    bool stickEdge = stickOn && !prevStickOn_, btnEdge = btnOn && !prevBtnOn_;
    bool stickRel = !stickOn && prevStickOn_, btnRel = !btnOn && prevBtnOn_;
    if (charging_) {
        if (stickRel || btnRel) charging_ = false;   // first release fires
    } else if (stickEdge || btnEdge) {
        charging_ = true;
    }
    prevStickOn_ = stickOn;
    prevBtnOn_ = btnOn;
    return charging_;
}

void InputMap::reset(int W, int H) {
    *this = InputMap();
    aimX_ = W * 0.5f;
    cursor_.x = W * 0.5f;
    cursor_.y = H * 0.5f;
}

void InputMap::update(const PadState& s, const InputCfg& cfg, int scheme, InputContext ctx, float dt, int W, int H) {
    const GamepadCfg& g = cfg.gamepad;
    active_ = false;
    if (!ctxInit_) { ctxInit_ = true; lastCtx_ = ctx; }
    if (ctx != lastCtx_) {  // context switch: lift the finger, keep the aim
        finger_.down = false;
        pullX_ = pullY_ = 0;
        wasPress_ = true;   // require a fresh press in the new context
        charging_ = false;
        prevStickOn_ = prevBtnOn_ = true;  // stick mode: same rule
        lastCtx_ = ctx;
    }

    float lx = s.lx, ly = s.ly, rx = s.rx, ry = s.ry;
    applyDeadzone(lx, ly, g.deadzoneLeft, g.outerDeadzone, g.curve);
    applyDeadzone(rx, ry, g.deadzoneRight, g.outerDeadzone, g.curve);
    float dx = 0, dy = 0;  // d-pad + keyboard digital direction
    if (s.down(PB_DLEFT)) dx -= 1;
    if (s.down(PB_DRIGHT)) dx += 1;
    if (s.down(PB_DUP)) dy -= 1;
    if (s.down(PB_DDOWN)) dy += 1;
    dx = clampf(dx + s.kbX, -1, 1);
    dy = clampf(dy + s.kbY, -1, 1);

    // gyro (yaw -> x, pitch -> y), "hold" mode only while the gyro_hold binding is held
    float gx = 0, gy = 0;
    if (g.gyro.enabled) {
        auto it = g.bindings.find("gyro_hold");
        bool on = g.gyro.mode != "hold" || (it != g.bindings.end() && bindingDown(s, it->second));
        if (on) {
            gx = -s.gyroY * kGyroPxPerRad * g.gyro.sensitivity * dt * (g.gyro.invertX ? -1.f : 1.f);
            gy = -s.gyroX * kGyroPxPerRad * g.gyro.sensitivity * dt * (g.gyro.invertY ? -1.f : 1.f);
            if (std::fabs(s.gyroX) + std::fabs(s.gyroY) > 0.15f) active_ = true;
        }
    }

    auto bind = [&](const char* action) {
        auto it = g.bindings.find(action);
        return it != g.bindings.end() && bindingDown(s, it->second);
    };
    bool press = bind("press") || bind("press_alt") || s.kbPress;
    bool recenter = bind("recenter") || s.kbRecenter;
    bool anyButton = s.buttons != 0 || s.lt > 0.5f || s.rt > 0.5f || s.kbPress || s.kbRecenter;
    if (lx || ly || rx || ry || dx || dy || anyButton) active_ = true;

    if (ctx == InputContext::Menu) {
        float mx = clampf(lx + rx + dx, -1, 1), my = clampf(ly + ry + dy, -1, 1);
        cursor_.x = clampf(cursor_.x + mx * kCursorSpeed * g.cursorSpeed * dt, 0, (float)W - 1);
        cursor_.y = clampf(cursor_.y + my * kCursorSpeed * g.cursorSpeed * dt, 0, (float)H - 1);
        bool tap = bind("menu_tap") || s.kbPress;
        if (active_) cursor_.visible = true;
        cursor_.tap = tap && !wasPress_ ? true : (tap && cursor_.tap);
        finger_.down = cursor_.tap;
        finger_.x = cursor_.x;
        finger_.y = cursor_.y;
        wasPress_ = tap;
    } else {
        cursor_.visible = false;
        cursor_.tap = false;
        if (scheme == 0) {
            // ---- precision
            if (recenter && !wasRecenter_) aimX_ = W * 0.5f;
            bool stickPress = false;
            if (g.aimMode == "stick") {
                // the shot goes where the stick points: right stick (or left), tilt = charge, release = fire
                float sx = rx, sy = ry;
                if (sx * sx + sy * sy < lx * lx + ly * ly) { sx = lx; sy = ly; }
                updateStick(sx, sy);
                if (stickHeld_) aimX_ = precisionXForAngle(aimAngleFromStick(stickDirX_, stickDirY_), W);
                aimX_ += (dx * kSlideSpeed * g.sensitivity * dt);
                stickPress = true;
            } else if (g.aimMode == "direct") {
                float sx = rx, sy = ry;
                if (sx == 0 && sy == 0) { sx = lx; sy = ly; }
                if (sx != 0 || sy != 0) aimX_ = precisionXForAngle(aimAngleFromStick(sx, sy), W);
                aimX_ += (dx * kSlideSpeed * g.sensitivity * dt);
            } else {
                aimX_ += (lx + dx + rx) * kSlideSpeed * g.sensitivity * dt;
            }
            aimX_ += gx;
            aimX_ = clampf(aimX_, 0, (float)W - 1);
            if (stickPress) {
                finger_.down = stickCharge(stickHeld_, press);   // first release (stick or button) fires
            } else {
                finger_.down = press && !(wasPress_ && !finger_.down);
            }
            finger_.x = aimX_;
            finger_.y = kPrecisionY;
        } else {
            // ---- slingshot (and scheme 2): start inside the 50 px circle around the screen centre,
            // then pull away (up to W * 0.33 * 1.1 px) -- release fires.
            const float maxPull = W * 0.33f * 1.1f * g.sensitivity;
            float sx = clampf(lx + rx + dx, -1, 1), sy = clampf(ly + ry + dy, -1, 1);
            if (g.aimMode == "stick") {
                // the shot follows the stick: put the finger on the OPPOSITE side of the pivot
                // (W/2, H-160) at a distance set by the peak tilt; the first held frame touches the
                // pivot itself (inside the original's 50 px start circle), release = fire.
                updateStick(sx, sy);
                bool held = stickCharge(stickHeld_, press);   // first release (stick or button) fires
                bool chargeStart = held && !finger_.down;
                if (chargeStart) { pullX_ = pullY_ = 0; }     // first frame: touch the pivot itself
                else if (held && stickHeld_) {
                    float dist = std::max(stickPeak_, 0.15f) * maxPull;
                    pullX_ = -stickDirX_ * dist;
                    pullY_ = -stickDirY_ * dist;
                }
                finger_.down = held;
                finger_.x = clampf(W * 0.5f + pullX_, 0, (float)W - 1);
                finger_.y = clampf((float)(H - 160) + pullY_, 0, (float)H - 1);
                wasPress_ = held;
                wasRecenter_ = recenter;
                idle_ = active_ ? 0 : idle_ + dt;
                return;
            }
            if (press) {
                if (!finger_.down) { pullX_ = pullY_ = 0; gyroAccX_ = gyroAccY_ = 0; }
                gyroAccX_ += gx;
                gyroAccY_ += gy;
                pullX_ = sx * maxPull + gyroAccX_;
                pullY_ = sy * maxPull + gyroAccY_;
                float m = std::sqrt(pullX_ * pullX_ + pullY_ * pullY_);
                if (m > maxPull && m > 0) { pullX_ *= maxPull / m; pullY_ *= maxPull / m; }
            }
            finger_.down = press && !(wasPress_ && !finger_.down);
            finger_.x = clampf(W * 0.5f + pullX_, 0, (float)W - 1);
            finger_.y = clampf(H * 0.5f + pullY_, 0, (float)H - 1);
        }
        wasPress_ = press;
    }
    wasRecenter_ = recenter;
    idle_ = active_ ? 0 : idle_ + dt;
    if (ctx == InputContext::Menu && idle_ > 3.0) cursor_.visible = false;
}

}  // namespace zig
