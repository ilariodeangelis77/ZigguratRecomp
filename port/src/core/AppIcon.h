// zigcore: the port's own window icon (original pixel-art "Z", see tools/make_icon.py).
#pragma once
struct SDL_Window;
namespace zig {
// Sets the 64x64 icon on an SDL window (taskbar/title bar/alt-tab on all platforms).
void setWindowIcon(SDL_Window* window);
}  // namespace zig
