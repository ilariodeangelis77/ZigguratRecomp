// Controller support for the game (LAUNCHER_PLAN.md §7.4 / §8.4).
// SDL3 gamepads + keyboard -> zig::InputMap -> ONE virtual finger fed into the existing touch path
// (plat::Touch), so the decompiled game logic sees exactly the touches it saw on the iPhone.
// Menus get a cursor drawn with the game's own reticle sprite; the "menu tap" button taps there.
#include "platform/pad.h"
#include "core/TestHooks.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/InputMap.h"
#include "platform/platform.h"

namespace plat {
namespace {

zig::InputCfg gCfg;
zig::InputMap gMap;
std::function<int(int&)> gContext;          // returns 0 = gameplay, 1 = menu; writes the control scheme
std::vector<SDL_Gamepad*> gPads;
SDL_Gamepad* gActive = nullptr;             // pad currently driving the finger
bool gPadSource = false;                    // last used device: pad/keyboard (true) or mouse/touch (false)
bool gFingerDown = false;                   // what we last reported through Touch
Uint64 gLastNs = 0;
bool gGyroOn = false;

// ---- ZIGGURAT_PAD test hook: a scripted virtual controller (no OS input involved).
struct PadStep { double t; std::string kind, name; float a = 0, b = 0, c = 0; bool done = false; };
std::vector<PadStep> gScript;
bool gScriptLoaded = false;
double gScriptStart = -1;
zig::PadState gFake;

void loadScript() {
    gScriptLoaded = true;
    const char* env = zig::testEnv("ZIGGURAT_PAD");
    if (!env) return;
    std::string s = env;
    size_t p = 0;
    while (p < s.size()) {
        size_t e = s.find(';', p);
        if (e == std::string::npos) e = s.size();
        std::string item = s.substr(p, e - p);
        p = e + 1;
        std::vector<std::string> f;
        size_t q = 0;
        while (q <= item.size()) {
            size_t c = item.find(':', q);
            if (c == std::string::npos) c = item.size();
            f.push_back(item.substr(q, c - q));
            q = c + 1;
        }
        if (f.size() < 3) continue;
        PadStep st;
        st.t = atof(f[0].c_str()) / 1000.0;
        st.kind = f[1];
        st.name = f[2];
        if (st.kind == "btn" && f.size() >= 4) st.a = f[3] == "down" ? 1.f : 0.f;
        else if (st.kind == "axis" && f.size() >= 4) st.a = (float)atof(f[3].c_str());
        else if (st.kind == "gyro" && f.size() >= 5) {
            st.a = (float)atof(f[2].c_str()); st.b = (float)atof(f[3].c_str()); st.c = (float)atof(f[4].c_str());
        }
        gScript.push_back(st);
    }
    SDL_Log("pad: ZIGGURAT_PAD script with %d steps", (int)gScript.size());
}

void runScript(double now) {
    if (!gScriptLoaded) loadScript();
    if (gScript.empty()) return;
    if (gScriptStart < 0) gScriptStart = now;
    gFake.connected = true;
    for (auto& st : gScript) {
        if (st.done || now - gScriptStart < st.t) continue;
        st.done = true;
        if (st.kind == "btn") {
            int b = zig::buttonFromName(st.name);
            if (st.name == "rt") gFake.rt = st.a;
            else if (st.name == "lt") gFake.lt = st.a;
            else if (b >= 0) {
                if (st.a > 0) gFake.buttons |= 1u << b;
                else gFake.buttons &= ~(1u << b);
            }
        } else if (st.kind == "axis") {
            float* ax = st.name == "lx" ? &gFake.lx : st.name == "ly" ? &gFake.ly : st.name == "rx" ? &gFake.rx
                      : st.name == "ry" ? &gFake.ry : st.name == "lt" ? &gFake.lt : st.name == "rt" ? &gFake.rt : nullptr;
            if (ax) *ax = st.a;
        } else if (st.kind == "gyro") {
            gFake.gyroX = st.a; gFake.gyroY = st.b; gFake.gyroZ = st.c;
        }
    }
}

std::string guidString(SDL_Gamepad* p) {
    char buf[64];
    SDL_GUIDToString(SDL_GetJoystickGUIDForID(SDL_GetGamepadID(p)), buf, sizeof buf);
    return buf;
}

void enableGyro(SDL_Gamepad* p, bool on) {
    if (p && SDL_GamepadHasSensor(p, SDL_SENSOR_GYRO)) SDL_SetGamepadSensorEnabled(p, SDL_SENSOR_GYRO, on);
}

void readPad(SDL_Gamepad* p, zig::PadState& s) {
    s.connected = true;
    auto ax = [&](SDL_GamepadAxis a) { return SDL_GetGamepadAxis(p, a) / 32767.0f; };
    s.lx = ax(SDL_GAMEPAD_AXIS_LEFTX); s.ly = ax(SDL_GAMEPAD_AXIS_LEFTY);
    s.rx = ax(SDL_GAMEPAD_AXIS_RIGHTX); s.ry = ax(SDL_GAMEPAD_AXIS_RIGHTY);
    s.lt = ax(SDL_GAMEPAD_AXIS_LEFT_TRIGGER); s.rt = ax(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    static const SDL_GamepadButton map[zig::PB_COUNT] = {
        SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH,
        SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_GUIDE, SDL_GAMEPAD_BUTTON_START, SDL_GAMEPAD_BUTTON_LEFT_STICK,
        SDL_GAMEPAD_BUTTON_RIGHT_STICK, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
        SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT};
    s.buttons = 0;
    for (int i = 0; i < zig::PB_COUNT; i++)
        if (SDL_GetGamepadButton(p, map[i])) s.buttons |= 1u << i;
    if (gGyroOn && SDL_GamepadSensorEnabled(p, SDL_SENSOR_GYRO)) {
        float g[3] = {0, 0, 0};
        SDL_GetGamepadSensorData(p, SDL_SENSOR_GYRO, g, 3);
        s.gyroX = g[0]; s.gyroY = g[1]; s.gyroZ = g[2];
    }
}

bool padHasActivity(const zig::PadState& s) {
    return s.buttons || s.lt > 0.3f || s.rt > 0.3f || std::abs(s.lx) > 0.3f || std::abs(s.ly) > 0.3f ||
           std::abs(s.rx) > 0.3f || std::abs(s.ry) > 0.3f;
}

void merge(zig::PadState& into, const zig::PadState& s) {
    if (!s.connected) return;
    into.connected = true;
    auto pick = [](float a, float b) { return std::abs(b) > std::abs(a) ? b : a; };
    into.lx = pick(into.lx, s.lx); into.ly = pick(into.ly, s.ly);
    into.rx = pick(into.rx, s.rx); into.ry = pick(into.ry, s.ry);
    into.lt = std::max(into.lt, s.lt); into.rt = std::max(into.rt, s.rt);
    into.buttons |= s.buttons;
    into.gyroX += s.gyroX; into.gyroY += s.gyroY; into.gyroZ += s.gyroZ;
}

bool keyDown(const bool* ks, const std::string& action) {
    auto it = gCfg.keyboard.keys.find(action);
    if (it == gCfg.keyboard.keys.end() || it->second.empty()) return false;
    SDL_Keycode k = SDL_GetKeyFromName(it->second.c_str());
    if (k == SDLK_UNKNOWN) return false;
    SDL_Scancode sc = SDL_GetScancodeFromKey(k, nullptr);
    return sc != SDL_SCANCODE_UNKNOWN && ks[sc];
}

}  // namespace

void configureInput(const zig::InputCfg& cfg) { gCfg = cfg; }
void setInputContextProvider(std::function<int(int&)> f) { gContext = std::move(f); }

void padInit() {
    gMap.reset(480, 320);
    gGyroOn = gCfg.gamepad.gyro.enabled;
    int n = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&n);
    for (int i = 0; i < n; i++)
        if (SDL_Gamepad* p = SDL_OpenGamepad(ids[i])) { gPads.push_back(p); enableGyro(p, gGyroOn); }
    SDL_free(ids);
    SDL_Log("pad: %d controller(s) at start", (int)gPads.size());
}

void padShutdown() {
    for (auto* p : gPads) SDL_CloseGamepad(p);
    gPads.clear();
    gActive = nullptr;
}

void padHandleEvent(const SDL_Event& e) {
    switch (e.type) {
        case SDL_EVENT_GAMEPAD_ADDED:
            if (!SDL_GetGamepadFromID(e.gdevice.which)) {
                if (SDL_Gamepad* p = SDL_OpenGamepad(e.gdevice.which)) {
                    gPads.push_back(p);
                    enableGyro(p, gGyroOn);
                    SDL_Log("pad: connected %s", SDL_GetGamepadName(p));
                }
            }
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            for (size_t i = 0; i < gPads.size(); i++)
                if (SDL_GetGamepadID(gPads[i]) == e.gdevice.which) {
                    if (gActive == gPads[i]) gActive = nullptr;
                    SDL_CloseGamepad(gPads[i]);
                    gPads.erase(gPads.begin() + (long)i);
                    break;
                }
            break;
        default: break;
    }
}

void padUpdate(Touch& t, bool pointerUsed, int W, int H) {
    Uint64 ns = SDL_GetTicksNS();
    float dt = gLastNs ? (float)((ns - gLastNs) / 1e9) : 1.0f / 30;
    if (dt > 0.25f) dt = 0.25f;
    gLastNs = ns;
    runScript(ns / 1e9);

    // Collect controller state: configured pad (guid) or the most recently active one.
    zig::PadState s;
    for (auto* p : gPads) {
        zig::PadState ps;
        readPad(p, ps);
        bool wanted = gCfg.gamepad.guid == "auto" || gCfg.gamepad.guid == guidString(p);
        if (!wanted) continue;
        if (padHasActivity(ps)) gActive = p;
        if (gActive == p || (!gActive && gCfg.gamepad.guid != "auto")) merge(s, ps);
    }
    if (!gActive && !gPads.empty() && gCfg.gamepad.guid == "auto") { zig::PadState ps; readPad(gPads[0], ps); merge(s, ps); }
    merge(s, gFake);
    // Keyboard drives the same virtual finger.
    const bool* ks = SDL_GetKeyboardState(nullptr);
    s.kbPress = keyDown(ks, "press");
    s.kbRecenter = keyDown(ks, "recenter");
    s.kbX = (keyDown(ks, "right") ? 1.f : 0.f) - (keyDown(ks, "left") ? 1.f : 0.f);
    s.kbY = (keyDown(ks, "down") ? 1.f : 0.f) - (keyDown(ks, "up") ? 1.f : 0.f);

    int scheme = 0;
    int ctx = gContext ? gContext(scheme) : 1;
    gMap.update(s, gCfg, scheme, ctx == 0 ? zig::InputContext::Gameplay : zig::InputContext::Menu, dt, W, H);

    if (pointerUsed) {          // mouse / real touch takes over immediately
        if (gPadSource && gFingerDown && !t.down) gFingerDown = false;
        gPadSource = false;
        gMap.hideCursor();
        if (gFingerDown) gFingerDown = false;
        return;
    }
    if (gMap.activeThisFrame()) gPadSource = true;
    if (!gPadSource) return;

    const zig::VirtualFinger& f = gMap.finger();
    if (f.down && !gFingerDown) {
        t.x = f.x; t.y = f.y; t.down = true; t.began = true;
    } else if (f.down && gFingerDown) {
        if (t.x != f.x || t.y != f.y) { t.x = f.x; t.y = f.y; t.moved = true; }
        t.down = true;
    } else if (!f.down && gFingerDown) {
        t.x = f.x; t.y = f.y; t.down = false; t.ended = true;
    }
    gFingerDown = f.down;
}

bool padCursor(float& x, float& y) {
    if (!gPadSource) return false;
    const zig::MenuCursor& c = gMap.cursor();
    if (!c.visible) return false;
    x = c.x; y = c.y;
    return true;
}

void padRumbleForSound(const std::string& name) {
    float k = gCfg.gamepad.rumble;  // ENHANCEMENT ONLY: the original game never vibrates
    if (k <= 0 || !gActive) return;
    Uint16 lo = 0, hi = 0;
    Uint32 ms = 0;
    if (name == "playerHit.wav") { lo = 0xC000; hi = 0x8000; ms = 300; }
    else if (name == "gunChargeUp.wav") { lo = 0x2000; hi = 0x4000; ms = 80; }
    else if (name == "scream.wav" || name == "deathBlow.wav") { lo = 0xFFFF; hi = 0xC000; ms = 600; }
    else if (name == "explode.wav") { lo = 0x6000; hi = 0x3000; ms = 120; }
    else return;
    SDL_RumbleGamepad(gActive, (Uint16)(lo * k), (Uint16)(hi * k), ms);
}

bool padMouseAsTouch() { return gCfg.mouseAsTouch; }

std::string padName() { return gActive ? SDL_GetGamepadName(gActive) : (gPads.empty() ? "" : SDL_GetGamepadName(gPads[0])); }

}  // namespace plat
