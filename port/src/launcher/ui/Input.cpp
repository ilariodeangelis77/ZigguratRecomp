#include "launcher/ui/Input.h"
#include "core/TestHooks.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "launcher/ui/Gfx.h"

namespace zui {

Input& input() {
    static Input i;
    return i;
}

void Input::beginFrame() {
    std::fill(std::begin(pressed), std::end(pressed), false);
    pointerMoved = pointerPressed = pointerReleased = false;
    wheel = 0;
    keys.clear();
}

void Input::setDevice(SDL_Gamepad* p) {
    lastPad_ = p;
    usingPointer = false;
    switch (SDL_GetGamepadType(p)) {
        case SDL_GAMEPAD_TYPE_PS3:
        case SDL_GAMEPAD_TYPE_PS4:
        case SDL_GAMEPAD_TYPE_PS5: device = Device::PlayStation; break;
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: device = Device::Nintendo; break;
        default: device = Device::Xbox; break;
    }
}

void Input::handle(const SDL_Event& e) {
    switch (e.type) {
        case SDL_EVENT_QUIT: quit = true; break;
        case SDL_EVENT_GAMEPAD_ADDED: {
            if (SDL_Gamepad* p = SDL_OpenGamepad(e.gdevice.which)) pads_.push_back(p);
            break;
        }
        case SDL_EVENT_GAMEPAD_REMOVED: {
            for (size_t i = 0; i < pads_.size(); i++)
                if (SDL_GetGamepadID(pads_[i]) == e.gdevice.which) {
                    if (lastPad_ == pads_[i]) lastPad_ = nullptr;
                    SDL_CloseGamepad(pads_[i]);
                    pads_.erase(pads_.begin() + i);
                    break;
                }
            break;
        }
        case SDL_EVENT_KEY_DOWN: {
            device = Device::Keyboard;
            usingPointer = false;
            keys.insert(e.key.key);
            switch (e.key.key) {
                case SDLK_UP: case SDLK_W: press(Act::Up); break;
                case SDLK_DOWN: case SDLK_S: press(Act::Down); break;
                case SDLK_LEFT: case SDLK_A: press(Act::Left); break;
                case SDLK_RIGHT: case SDLK_D: press(Act::Right); break;
                case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: press(Act::Ok); break;
                case SDLK_ESCAPE: case SDLK_BACKSPACE: press(Act::Back); break;
                case SDLK_Q: press(Act::TabPrev); break;
                case SDLK_E: press(Act::TabNext); break;
                case SDLK_TAB: press((e.key.mod & SDL_KMOD_SHIFT) ? Act::Up : Act::Down); break;
                case SDLK_F5: press(Act::Start); break;
                case SDLK_R: press(Act::Y); break;
                default: break;
            }
            break;
        }
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
            SDL_Gamepad* p = SDL_GetGamepadFromID(e.gbutton.which);
            if (p) setDevice(p);
            switch ((SDL_GamepadButton)e.gbutton.button) {
                case SDL_GAMEPAD_BUTTON_DPAD_UP: press(Act::Up); held_[0] = {true, 0}; break;
                case SDL_GAMEPAD_BUTTON_DPAD_DOWN: press(Act::Down); held_[1] = {true, 0}; break;
                case SDL_GAMEPAD_BUTTON_DPAD_LEFT: press(Act::Left); held_[2] = {true, 0}; break;
                case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: press(Act::Right); held_[3] = {true, 0}; break;
                case SDL_GAMEPAD_BUTTON_SOUTH: press(Act::Ok); break;    // A / Cross / B(Nintendo, positional)
                case SDL_GAMEPAD_BUTTON_EAST: press(Act::Back); break;
                case SDL_GAMEPAD_BUTTON_WEST: press(Act::X); break;
                case SDL_GAMEPAD_BUTTON_NORTH: press(Act::Y); break;
                case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: press(Act::TabPrev); break;
                case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: press(Act::TabNext); break;
                case SDL_GAMEPAD_BUTTON_START: press(Act::Start); break;
                default: break;
            }
            break;
        }
        case SDL_EVENT_GAMEPAD_BUTTON_UP: {
            int b = e.gbutton.button;
            if (b >= SDL_GAMEPAD_BUTTON_DPAD_UP && b <= SDL_GAMEPAD_BUTTON_DPAD_RIGHT)
                held_[b - SDL_GAMEPAD_BUTTON_DPAD_UP].down = false;
            break;
        }
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            float v = e.gaxis.value / 32767.0f;
            if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) axisX_ = v;
            if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) axisY_ = v;
            if (std::fabs(v) > 0.5f)
                if (SDL_Gamepad* p = SDL_GetGamepadFromID(e.gaxis.which)) setDevice(p);
            break;
        }
        case SDL_EVENT_MOUSE_MOTION: {
            gfx().toCanvas(e.motion.x, e.motion.y, px, py);
            pointerMoved = true;
            usingPointer = true;
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            if (e.button.button != SDL_BUTTON_LEFT) break;
            gfx().toCanvas(e.button.x, e.button.y, px, py);
            usingPointer = true;
            device = Device::Keyboard;
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) { pointerDown = true; pointerPressed = true; }
            else { pointerDown = false; pointerReleased = true; }
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL: wheel += e.wheel.y; usingPointer = true; break;
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_MOTION: {
            int ww, wh;
            SDL_GetWindowSize(gfx().window(), &ww, &wh);
            gfx().toCanvas(e.tfinger.x * ww, e.tfinger.y * wh, px, py);
            usingPointer = true;
            pointerMoved = true;
            if (e.type == SDL_EVENT_FINGER_DOWN) { pointerDown = true; pointerPressed = true; }
            if (e.type == SDL_EVENT_FINGER_UP) { pointerDown = false; pointerReleased = true; }
            break;
        }
        default: break;
    }
}

void Input::endFrame(double now) {
    // Left stick as a d-pad with repeat; d-pad repeat.
    const double first = 0.35, rep = 0.09;
    bool stick[4] = {axisY_ < -0.5f, axisY_ > 0.5f, axisX_ < -0.5f, axisX_ > 0.5f};
    static bool stickPrev[4] = {};
    static double stickNext[4] = {};
    for (int d = 0; d < 4; d++) {
        Act a = (Act)d;  // Up, Down, Left, Right share order
        if (stick[d] && !stickPrev[d]) { press(a); stickNext[d] = now + first; }
        else if (stick[d] && now >= stickNext[d]) { press(a); stickNext[d] = now + rep; }
        stickPrev[d] = stick[d];
        if (held_[d].down) {
            if (held_[d].next == 0) held_[d].next = now + first;
            else if (now >= held_[d].next) { press(a); held_[d].next = now + rep; }
        }
    }
    // Scripted input for automated tests.
    if (!scriptLoaded_) {
        scriptLoaded_ = true;
        if (const char* s = zig::testEnv("ZIGGURAT_UI_SCRIPT")) {
            std::string spec = s;
            size_t i = 0;
            while (i < spec.size()) {
                size_t j = spec.find(';', i);
                std::string item = spec.substr(i, j == std::string::npos ? std::string::npos : j - i);
                size_t c = item.find(':');
                if (c != std::string::npos) script_.push_back({atof(item.substr(0, c).c_str()) / 1000.0, item.substr(c + 1)});
                if (j == std::string::npos) break;
                i = j + 1;
            }
        }
    }
    if (start_ < 0) start_ = now;
    for (auto& st : script_) {
        if (st.done || now - start_ < st.t) continue;
        st.done = true;
        const std::string& w = st.what;
        static const char* names[] = {"up", "down", "left", "right", "ok", "back", "lb", "rb", "start", "x", "y"};
        for (int a = 0; a < (int)Act::Count; a++)
            if (w == names[a]) { press((Act)a); usingPointer = false; }
        if (w.rfind("tap:", 0) == 0) {
            float x = 0, y = 0;
            sscanf(w.c_str() + 4, "%f:%f", &x, &y);
            px = x; py = y;
            pointerMoved = pointerPressed = pointerReleased = true;
            usingPointer = true;
        } else if (w.rfind("key:", 0) == 0) {
            keys.insert(SDL_GetKeyFromName(w.c_str() + 4));
        } else if (w.rfind("pad:", 0) == 0) {
            std::string k = w.substr(4);
            device = k == "ps" ? Device::PlayStation : k == "nintendo" ? Device::Nintendo
                   : k == "xbox" ? Device::Xbox : Device::Keyboard;
            usingPointer = false;
        }
    }
}

void Input::rumble(float strength, int ms) {
    if (!lastPad_) return;
    Uint16 v = (Uint16)(std::clamp(strength, 0.0f, 1.0f) * 65535);
    SDL_RumbleGamepad(lastPad_, v, v, (Uint32)ms);
}

}  // namespace zui

namespace zui {
std::string Input::gamepadName() const {
    SDL_Gamepad* p = lastPad_ ? lastPad_ : (pads_.empty() ? nullptr : pads_.front());
    if (!p) return "";
    const char* n = SDL_GetGamepadName(p);
    return n ? n : "Gamepad";
}
}  // namespace zui
