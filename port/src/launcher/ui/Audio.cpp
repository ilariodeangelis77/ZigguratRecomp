#include "launcher/ui/Audio.h"
#include "core/FileIO.h"

#include <SDL3/SDL.h>

#include <map>
#include <memory>
#include <vector>

#include "dr_mp3.h"  // implementations live in zigengine (platform.cpp)
#include "dr_wav.h"

namespace zui {
namespace audio {
namespace {
SDL_AudioDeviceID gDev = 0;
SDL_AudioSpec gDevSpec{};
struct Clip {
    SDL_AudioSpec spec{};
    std::vector<int16_t> pcm;
};
std::map<std::string, std::unique_ptr<Clip>> gClips;
std::vector<SDL_AudioStream*> gVoices;
float gMaster = 1, gMusic = 1, gSfx = 1;

struct Music {
    SDL_AudioStream* s = nullptr;
    Clip clip;
    size_t cursor = 0;
    float gain = 0.6f;
    std::string path;
} gMus;

const Clip* loadWav(const std::string& path) {
    auto it = gClips.find(path);
    if (it != gClips.end()) return it->second.get();
    unsigned ch, rate;
    drwav_uint64 frames;
    std::vector<uint8_t> bytes;
    int16_t* d = (zig::readFile(path, bytes) && !bytes.empty())
        ? drwav_open_memory_and_read_pcm_frames_s16(bytes.data(), bytes.size(), &ch, &rate, &frames, nullptr) : nullptr;
    if (!d) {
        gClips[path] = nullptr;
        return nullptr;
    }
    auto c = std::make_unique<Clip>();
    c->spec = {SDL_AUDIO_S16, (int)ch, (int)rate};
    c->pcm.assign(d, d + frames * ch);
    drwav_free(d, nullptr);
    return (gClips[path] = std::move(c)).get();
}
}  // namespace

bool init() {
    if (gDev) return true;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;
    SDL_AudioSpec want{SDL_AUDIO_F32, 2, 48000};
    gDev = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &want);
    if (!gDev) return false;
    SDL_GetAudioDeviceFormat(gDev, &gDevSpec, nullptr);
    return true;
}

void shutdown() {
    for (auto* s : gVoices) SDL_DestroyAudioStream(s);
    gVoices.clear();
    if (gMus.s) SDL_DestroyAudioStream(gMus.s);
    gMus = Music{};
    if (gDev) SDL_CloseAudioDevice(gDev);
    gDev = 0;
}

void setVolumes(float master, float music, float sfx) {
    gMaster = master;
    gMusic = music;
    gSfx = sfx;
    if (gMus.s) SDL_SetAudioStreamGain(gMus.s, gMus.gain * gMaster * gMusic);
}

void play(const std::string& path, float gain) {
    if (!gDev) return;
    const Clip* c = loadWav(path);
    if (!c) return;
    SDL_AudioStream* s = SDL_CreateAudioStream(&c->spec, &gDevSpec);
    if (!s) return;
    SDL_SetAudioStreamGain(s, gain * gMaster * gSfx);
    SDL_PutAudioStreamData(s, c->pcm.data(), (int)(c->pcm.size() * 2));
    SDL_FlushAudioStream(s);
    SDL_BindAudioStream(gDev, s);
    gVoices.push_back(s);
}

void music(const std::string& path, float gain) {
    if (!gDev) return;
    if (gMus.s) {
        SDL_DestroyAudioStream(gMus.s);
        gMus.s = nullptr;
    }
    gMus.path = path;
    if (path.empty()) return;
    drmp3_config cfg;
    drmp3_uint64 frames;
    std::vector<uint8_t> bytes;
    int16_t* d = (zig::readFile(path, bytes) && !bytes.empty())
        ? drmp3_open_memory_and_read_pcm_frames_s16(bytes.data(), bytes.size(), &cfg, &frames, nullptr) : nullptr;
    if (!d) return;
    gMus.clip.spec = {SDL_AUDIO_S16, (int)cfg.channels, (int)cfg.sampleRate};
    gMus.clip.pcm.assign(d, d + frames * cfg.channels);
    drmp3_free(d, nullptr);
    gMus.cursor = 0;
    gMus.gain = gain;
    gMus.s = SDL_CreateAudioStream(&gMus.clip.spec, &gDevSpec);
    SDL_SetAudioStreamGain(gMus.s, gain * gMaster * gMusic);
    SDL_BindAudioStream(gDev, gMus.s);
}

void update() {
    for (size_t i = 0; i < gVoices.size();) {
        if (SDL_GetAudioStreamQueued(gVoices[i]) <= 0) {
            SDL_DestroyAudioStream(gVoices[i]);
            gVoices.erase(gVoices.begin() + i);
        } else i++;
    }
    if (gMus.s && !gMus.clip.pcm.empty()) {
        const int ch = gMus.clip.spec.channels;
        const int target = gMus.clip.spec.freq / 2 * ch * 2;
        while (SDL_GetAudioStreamQueued(gMus.s) < target) {
            size_t total = gMus.clip.pcm.size();
            size_t n = std::min<size_t>(total - gMus.cursor, (size_t)gMus.clip.spec.freq / 10 * ch);
            SDL_PutAudioStreamData(gMus.s, gMus.clip.pcm.data() + gMus.cursor, (int)(n * 2));
            gMus.cursor += n;
            if (gMus.cursor >= total) gMus.cursor = 0;
        }
    }
}

}  // namespace audio
}  // namespace zui
