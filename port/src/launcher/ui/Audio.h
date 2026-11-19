// ZigUI sounds: tiny SDL3 stream mixer for UI effects (WAV) and optional looping music (MP3).
#pragma once
#include <string>

namespace zui {
namespace audio {
bool init();
void shutdown();
void setVolumes(float master, float music, float sfx);
void play(const std::string& wavPath, float gain = 1.0f);   // fire-and-forget
void music(const std::string& mp3Path, float gain = 0.6f);  // "" stops
void update();                                              // call once per frame
}  // namespace audio
}  // namespace zui
