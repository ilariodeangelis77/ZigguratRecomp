// Unit tests for zig::InputMap (controller -> virtual finger / menu cursor). Exit code = failures.
#include <SDL3/SDL_main.h>

#include <cmath>
#include <cstdio>

#include "core/InputMap.h"

using namespace zig;

static int fails = 0, passes = 0;
#define CHECK(cond) do { if (cond) passes++; else { fails++; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
static bool near(float a, float b, float eps = 0.01f) { return std::fabs(a - b) <= eps; }

// Game-side reference formulas (port/src/game/Player.cpp)
static float gamePrecisionAngle(float touchX) { return touchX / 480.0f * 270.0f + 135.0f; }

int main(int, char**) {
    InputCfg cfg;
    // ---- deadzone
    { float x = 0.1f, y = 0.0f; applyDeadzone(x, y, 0.15f, 0.95f, "linear"); CHECK(x == 0 && y == 0); }
    { float x = 1.0f, y = 0.0f; applyDeadzone(x, y, 0.15f, 0.95f, "linear"); CHECK(near(x, 1)); }
    { float x = 0.55f, y = 0.0f; applyDeadzone(x, y, 0.15f, 0.95f, "linear"); CHECK(near(x, 0.5f)); }
    { float x = 0.55f, y = 0.0f; applyDeadzone(x, y, 0.15f, 0.95f, "quadratic"); CHECK(near(x, 0.25f)); }
    { float x = 0.6f, y = 0.6f; applyDeadzone(x, y, 0.15f, 0.95f, "linear"); CHECK(near(std::sqrt(x*x+y*y), 0.873f, 0.01f)); CHECK(near(x, y)); }
    // ---- precision formula inverse
    for (float a : {135.f, 200.f, 270.f, 333.f, 405.f}) CHECK(near(gamePrecisionAngle(precisionXForAngle(a, 480)), a, 0.05f));
    CHECK(near(precisionXForAngle(100, 480), 0));
    CHECK(near(precisionXForAngle(500, 480), 480));
    // stick directions -> aim angles
    CHECK(near(aimAngleFromStick(0, -1), 270));   // up
    CHECK(near(aimAngleFromStick(-1, 0), 180));   // left
    CHECK(near(aimAngleFromStick(1, 0), 360));    // right
    CHECK(near(aimAngleFromStick(1, 1), 405));    // down-right clamps to right edge
    CHECK(near(aimAngleFromStick(-1, 1), 135));   // down-left clamps to left edge
    CHECK(near(aimAngleFromStick(0, 1), 135) || near(aimAngleFromStick(0, 1), 405));
    // ---- precision slide: hold RT, push left stick right for 1 s
    {
        InputMap m; m.reset();
        PadState s; s.connected = true;
        m.update(s, cfg, 0, InputContext::Gameplay, 1 / 30.f);
        CHECK(!m.finger().down); CHECK(near(m.aimX(), 240));
        s.rt = 1; m.update(s, cfg, 0, InputContext::Gameplay, 1 / 30.f);
        CHECK(m.finger().down); CHECK(near(m.finger().y, InputMap::kPrecisionY));
        s.lx = 1;
        for (int i = 0; i < 30; i++) m.update(s, cfg, 0, InputContext::Gameplay, 1 / 30.f);
        CHECK(near(m.finger().x, 240 + 240 * 1.0f * (0.95f - 0.15f) / (0.95f - 0.15f), 2) || m.finger().x > 470);
        CHECK(m.finger().x <= 479);  // clamp
        s.lx = 0; s.rt = 0; m.update(s, cfg, 0, InputContext::Gameplay, 1 / 30.f);
        CHECK(!m.finger().down);     // release fires
        // recenter
        s.buttons = 1u << PB_RSTICK; m.update(s, cfg, 0, InputContext::Gameplay, 1 / 30.f);
        CHECK(near(m.aimX(), 240));
    }
    // ---- precision direct: right stick up-left -> angle 225
    {
        InputCfg c = cfg; c.gamepad.aimMode = "direct";
        InputMap m; m.reset();
        PadState s; s.connected = true; s.rt = 1; s.rx = -0.7071f; s.ry = -0.7071f;
        m.update(s, c, 0, InputContext::Gameplay, 1 / 30.f);
        CHECK(m.finger().down);
        CHECK(near(gamePrecisionAngle(m.finger().x), 225, 0.5f));
    }
    // ---- slingshot: start inside the 50 px circle, pull down > 20 px, release
    {
        InputMap m; m.reset();
        PadState s; s.connected = true; s.rt = 1;
        m.update(s, cfg, 1, InputContext::Gameplay, 1 / 30.f);
        CHECK(m.finger().down);
        float d0 = std::hypot(m.finger().x - 240, m.finger().y - 160);
        CHECK(d0 <= 50);                                        // touchPivot inside the start circle
        s.ly = 1;
        m.update(s, cfg, 1, InputContext::Gameplay, 1 / 30.f);
        CHECK(m.finger().y - 160 > 20);                         // > 20 px drag starts charging
        CHECK(m.finger().y - 160 <= 480 * 0.33f * 1.1f + 0.5f);
        CHECK(near(m.finger().x, 240));
        s.rt = 0; m.update(s, cfg, 1, InputContext::Gameplay, 1 / 30.f);
        CHECK(!m.finger().down);
    }
    // ---- menu cursor + tap; context switch lifts the finger
    {
        InputMap m; m.reset();
        PadState s; s.connected = true; s.lx = 1;
        for (int i = 0; i < 15; i++) m.update(s, cfg, 0, InputContext::Menu, 1 / 30.f);
        CHECK(m.cursor().visible); CHECK(m.cursor().x > 240 + 100);
        s.lx = 0; s.buttons = 1u << PB_A; m.update(s, cfg, 0, InputContext::Menu, 1 / 30.f);
        CHECK(m.finger().down); CHECK(near(m.finger().x, m.cursor().x));
        m.update(s, cfg, 0, InputContext::Gameplay, 1 / 30.f);  // A still held after switching
        CHECK(!m.finger().down);
        s.buttons = 0; for (int i = 0; i < 100; i++) m.update(s, cfg, 0, InputContext::Menu, 1 / 30.f);
        CHECK(!m.cursor().visible);  // hidden after 3 s idle
    }
    // ---- bindings / keyboard
    {
        PadState s; s.lt = 0.7f; CHECK(bindingDown(s, "lt")); CHECK(!bindingDown(s, "rt"));
        s.buttons = 1u << PB_START; CHECK(bindingDown(s, "start")); CHECK(buttonFromName("dpad_left") == PB_DLEFT);
        InputMap m; m.reset(); PadState k; k.kbPress = true; k.kbX = -1;
        for (int i = 0; i < 10; i++) m.update(k, cfg, 0, InputContext::Gameplay, 1 / 30.f);
        CHECK(m.finger().down); CHECK(m.finger().x < 240);
    }
    // ---- gyro: yaw moves the precision finger only when enabled
    {
        InputCfg c = cfg; InputMap m; m.reset(); PadState s; s.connected = true; s.rt = 1; s.gyroY = -1;
        m.update(s, c, 0, InputContext::Gameplay, 0.1f); CHECK(near(m.aimX(), 240));
        c.gamepad.gyro.enabled = true; m.update(s, c, 0, InputContext::Gameplay, 0.1f); CHECK(m.aimX() > 260);
        c.gamepad.gyro.mode = "hold"; float before = m.aimX(); m.update(s, c, 0, InputContext::Gameplay, 0.1f);
        CHECK(near(m.aimX(), before));  // gyro_hold (LB) not held
    }
    // ---- aim_mode "stick": the shot follows the stick, tilt = charge, release = fire
    {
        InputCfg c = cfg; c.gamepad.aimMode = "stick";
        // precision: up-left tilt aims at 225 deg; springing back lifts the finger at that aim (fire)
        InputMap m; m.reset(); PadState s; s.connected = true; s.rx = -0.7071f; s.ry = -0.7071f;
        m.update(s, c, 0, InputContext::Gameplay, 1 / 30.f);
        CHECK(m.finger().down);
        CHECK(near(gamePrecisionAngle(m.finger().x), 225, 0.5f));
        s.rx = s.ry = 0; m.update(s, c, 0, InputContext::Gameplay, 1 / 30.f);
        CHECK(!m.finger().down);
        CHECK(near(gamePrecisionAngle(m.finger().x), 225, 0.5f));   // release keeps the aim
        // a small wobble below the engage threshold does not start a charge
        s.rx = 0.2f; m.update(s, c, 0, InputContext::Gameplay, 1 / 30.f); CHECK(!m.finger().down);
        // right tilt -> 360 deg (aim right)
        s.rx = 1; s.ry = 0; m.update(s, c, 0, InputContext::Gameplay, 1 / 30.f);
        CHECK(m.finger().down); CHECK(near(gamePrecisionAngle(m.finger().x), 360, 0.5f));
    }
    {
        InputCfg c = cfg; c.gamepad.aimMode = "stick";
        // slingshot: first frame touches the pivot (inside the 50 px start circle) ...
        InputMap m; m.reset(); PadState s; s.connected = true; s.lx = -0.7071f; s.ly = -0.7071f;
        m.update(s, c, 1, InputContext::Gameplay, 1 / 30.f);
        CHECK(m.finger().down);
        CHECK(near(m.finger().x, 240, 1) && near(m.finger().y, 160, 1));
        // ... then the finger moves OPPOSITE the stick, so pivot - finger points where the stick points
        m.update(s, c, 1, InputContext::Gameplay, 1 / 30.f);
        float vx = 240 - m.finger().x, vy = 160 - m.finger().y, len = std::sqrt(vx * vx + vy * vy);
        CHECK(len > 20);                                            // > 20 px drag starts charging
        CHECK(near(vx / len, -0.7071f, 0.01f) && near(vy / len, -0.7071f, 0.01f));
        float fx = m.finger().x, fy = m.finger().y;
        s.lx = s.ly = 0; m.update(s, c, 1, InputContext::Gameplay, 1 / 30.f);
        CHECK(!m.finger().down);                                    // release fires ...
        CHECK(near(m.finger().x, fx) && near(m.finger().y, fy));    // ... from the latched pull
    }
    // ---- stick mode + trigger held together: the FIRST release (stick or trigger) fires
    {
        InputCfg c = cfg; c.gamepad.aimMode = "stick";
        auto step = [&](InputMap& m, PadState& s, int scheme) { m.update(s, c, scheme, InputContext::Gameplay, 1 / 30.f); };
        for (int scheme : {0, 1}) {
            // (a) hold RT, tilt, let the stick spring back while RT is still held -> fires
            InputMap m; m.reset(); PadState s; s.connected = true;
            s.rt = 1; step(m, s, scheme); CHECK(m.finger().down);              // RT starts the charge
            s.rx = -0.7071f; s.ry = -0.7071f; s.lx = s.rx; s.ly = s.ry;
            step(m, s, scheme); step(m, s, scheme); CHECK(m.finger().down);     // aiming
            s.rx = s.ry = s.lx = s.ly = 0; step(m, s, scheme);
            CHECK(!m.finger().down);                                            // stick release fired (RT still held)
            step(m, s, scheme); CHECK(!m.finger().down);                        // no auto re-charge while RT stays held
            // re-tilting while RT is held starts a new charge (rapid fire with the stick alone)
            s.rx = 0.7071f; s.ry = -0.7071f; s.lx = s.rx; s.ly = s.ry; step(m, s, scheme); CHECK(m.finger().down);
            // (b) now release RT while the stick is still tilted -> fires too
            step(m, s, scheme); s.rt = 0; step(m, s, scheme);
            CHECK(!m.finger().down);
            step(m, s, scheme); CHECK(!m.finger().down);                        // stick still tilted: no re-charge
            // fresh RT press with the stick tilted starts again
            s.rt = 1; step(m, s, scheme); CHECK(m.finger().down);
        }
        // slingshot: a charge started by the trigger still touches the pivot first (start circle rule)
        InputMap m; m.reset(); PadState s; s.connected = true; s.lx = -0.7071f; s.ly = -0.7071f;
        step(m, s, 1); s.lx = s.ly = 0; step(m, s, 1);                         // tilt + release (fires)
        s.lx = -0.7071f; s.ly = -0.7071f; s.rt = 0; step(m, s, 1);              // tilt again -> charge
        CHECK(m.finger().down && near(m.finger().x, 240, 1) && near(m.finger().y, 160, 1));
    }
    std::printf("ziginputtest: %d passed, %d failed\n", passes, fails);
    return fails;
}
