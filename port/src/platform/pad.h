// Controller -> virtual finger for the game (see pad.cpp).
#pragma once
#include <SDL3/SDL.h>

#include <functional>
#include <string>

#include "core/Config.h"

namespace plat {
struct Touch;

void configureInput(const zig::InputCfg& cfg);           // before init()
// Context provider: return 0 = gameplay, 1 = menu; write the PlayState control scheme into `scheme`.
void setInputContextProvider(std::function<int(int& scheme)> f);

// internal (called by platform.cpp)
void padInit();
void padShutdown();
void padHandleEvent(const SDL_Event& e);
void padUpdate(Touch& t, bool pointerUsedThisFrame, int W, int H);
bool padCursor(float& x, float& y);                      // menu cursor position when visible
void padRumbleForSound(const std::string& name);         // enhancement (cfg rumble > 0)
std::string padName();
bool padMouseAsTouch();                                  // cfg input.mouse_as_touch
}  // namespace plat
