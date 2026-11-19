// ZigUI input: keyboard, SDL3 gamepads, mouse and touch merged into navigation actions.
// Test hook: env ZIGGURAT_UI_SCRIPT="ms:action;..." with actions
//   up down left right ok back lb rb start x y | tap:X:Y (canvas coords) | key:<SDL key name> | pad:xbox|ps|nintendo|keyboard
#pragma once
#include <SDL3/SDL.h>

#include <set>
#include <string>
#include <vector>

namespace zui {

enum class Act { Up, Down, Left, Right, Ok, Back, TabPrev, TabNext, Start, X, Y, Count };
enum class Device { Keyboard, Xbox, PlayStation, Nintendo };

struct Input {
    // per-frame edges (true for one frame, includes auto-repeat for directions)
    bool pressed[(int)Act::Count] = {};
    // pointer (canvas coordinates)
    float px = -1, py = -1;
    bool pointerMoved = false, pointerDown = false, pointerPressed = false, pointerReleased = false;
    float wheel = 0;
    std::set<SDL_Keycode> keys;  // keys pressed this frame
    Device device = Device::Keyboard;
    bool usingPointer = false;   // last interaction was mouse/touch
    bool quit = false;

    bool hit(Act a) const { return pressed[(int)a]; }
    bool key(SDL_Keycode k) const { return keys.count(k) != 0; }

    void beginFrame();                  // clears edges
    void handle(const SDL_Event& e);    // feed every SDL event
    void endFrame(double now);          // gamepad repeat + scripted input
    void rumble(float strength, int ms);
    std::string gamepadName() const;   // "" when no controller is connected
    SDL_Gamepad* activePad() const { return lastPad_ ? lastPad_ : (pads_.empty() ? nullptr : pads_.front()); }
    const std::vector<SDL_Gamepad*>& pads() const { return pads_; }
    void forceDevice(Device d) { device = d; }

private:
    struct Held { bool down = false; double next = 0; };
    Held held_[4];
    float axisX_ = 0, axisY_ = 0;
    std::vector<SDL_Gamepad*> pads_;
    SDL_Gamepad* lastPad_ = nullptr;
    struct Step { double t; std::string what; bool done = false; };
    std::vector<Step> script_;
    bool scriptLoaded_ = false;
    double start_ = -1;
    void press(Act a) { pressed[(int)a] = true; }
    void setDevice(SDL_Gamepad* p);
};

Input& input();

}  // namespace zui
