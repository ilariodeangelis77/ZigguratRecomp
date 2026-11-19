//
//  FlxG.cpp — FlxG.m (globals, audio, texture cache, camera follow)
//
//  Copyright Semi Secret Software 2009-2010. All rights reserved.
//  C++ port: MIT license (see ENGINE_LICENSE.txt).
//
//  PORT: OpenAL sources and the AVAudioPlayer music player map onto plat:: audio. AVAudioSession /
//  "other audio playing" / interruption handling has no desktop equivalent and is reduced to no-ops.
//
#include <SDL3/SDL.h>
#include "core/TestHooks.h"

#include <cstring>
#include <map>

#include "Flixel.h"

// PORT (test determinism): with ZIGGURAT_FIXED_DT the game runs faster than real time, so asking the
// audio device whether a sound is still playing would leak wall-clock time into game logic (e.g. the
// game-over sequence waits for scream.wav). In that mode FlxG::play/playing/stop use a virtual
// playback clock in game time, with each WAV's real duration. Normal play queries the device as before.
namespace {
bool fixedDtMode() {
    static const bool f = zig::testEnv("ZIGGURAT_FIXED_DT") != nullptr;
    return f;
}
double gSoundClock = 0;
std::map<std::string, double> gSoundEnds;  // name -> game-time end (infinity when looped)
double wavSeconds(const std::string& name) {
    static std::map<std::string, double> cache;
    auto it = cache.find(name);
    if (it != cache.end()) return it->second;
    double secs = 0;
    if (SDL_IOStream* io = SDL_IOFromFile(plat::assetPath(name).c_str(), "rb")) {
        unsigned char h[12];
        if (SDL_ReadIO(io, h, 12) == 12 && !memcmp(h, "RIFF", 4) && !memcmp(h + 8, "WAVE", 4)) {
            unsigned int rate = 0, align = 0;
            unsigned char ch[8];
            while (SDL_ReadIO(io, ch, 8) == 8) {
                unsigned int sz = ch[4] | (ch[5] << 8) | (ch[6] << 16) | ((unsigned)ch[7] << 24);
                if (!memcmp(ch, "fmt ", 4)) {
                    unsigned char f[16];
                    if (SDL_ReadIO(io, f, 16) != 16) break;
                    rate = f[4] | (f[5] << 8) | (f[6] << 16) | ((unsigned)f[7] << 24);
                    align = f[12] | (f[13] << 8);
                    SDL_SeekIO(io, (Sint64)sz - 16 + (sz & 1), SDL_IO_SEEK_CUR);
                } else if (!memcmp(ch, "data", 4)) {
                    if (rate && align) secs = (double)sz / align / rate;
                    break;
                } else {
                    SDL_SeekIO(io, (Sint64)sz + (sz & 1), SDL_IO_SEEK_CUR);
                }
            }
        }
        SDL_CloseIO(io);
    }
    return cache[name] = secs;
}
}  // namespace

FlxDevice FlxG::device;
float FlxG::soundEffectsMasterVolume = 1;
float FlxG::musicMasterVolume = 1;

static FlxGame* _game = nullptr;
static FlashPoint* _scrollTarget = nullptr;
static std::map<std::string, SemiSecretTexture*> _cache;
static float screenScale_ = 1;
static BOOL iPad_, retinaDisplay_, iPhone3GS_, iPhone3G_, iPhone1G_, iPodTouch1G_, iPodTouch2G_, iPodTouch3G_;
static float elapsed_;
static float maxElapsed_;
static float timeScale_;
static int width_;
static int height_;
static int level_;
static int score_;
static FlxSound* music_ = nullptr;
static FlxObject* followTarget = nullptr;
static BOOL pauseFollow_;
static FlashPoint* followLead = nullptr;
static float followLerp;
static FlashPoint* followMin = nullptr;
static FlashPoint* followMax = nullptr;
static FlashPoint* scroll_ = nullptr;
static FlxQuake* quake_ = nullptr;
static FlxFlash* flash_ = nullptr;
static FlxFade* fade_ = nullptr;
static bool audioPlayer = false;  // AVAudioPlayer exists
static float fadeOutDuration, fadeOutTimer, fadeOutVolume;
static BOOL musicPaused;
static FlxTouches* touches_ = nullptr;
static BOOL checkedVibration = false, doVibration = false;
std::string flxPendingMusic;  // performSelector:@selector(reallyPlayMusic:) afterDelay:0.0

// + (void) initialize
void FlxG::initialize() {
    static bool done = false;
    if (done) return;
    done = true;
    screenScale_ = device.retina ? 2.0f : 1.0f;
    iPad_ = device.iPad;
    retinaDisplay_ = device.retina;
    iPhone1G_ = device.platform == "iPhone1,1";
    iPhone3G_ = device.platform == "iPhone1,2";
    iPhone3GS_ = device.platform == "iPhone2,1";
    iPodTouch1G_ = device.platform == "iPod1,1";
    iPodTouch2G_ = device.platform == "iPod2,1";
    iPodTouch3G_ = device.platform == "iPod3,1";
    timeScale_ = 1;
    maxElapsed_ = 1.0 / 20;
    touches_ = new FlxTouches;
    // + (void) initAudio
    UserDefaults::standard().registerDefault("MusicVolume", "1");
    UserDefaults::standard().registerDefault("SoundEffectsVolume", "1");
    soundEffectsMasterVolume = UserDefaults::standard().getFloat("SoundEffectsVolume", 1);
    musicMasterVolume = UserDefaults::standard().getFloat("MusicVolume", 1);
}

float FlxG::screenScale() { return screenScale_; }
BOOL FlxG::iPad() { return iPad_; }
BOOL FlxG::retinaDisplay() { return retinaDisplay_; }
BOOL FlxG::iPhone1G() { return iPhone1G_; }
BOOL FlxG::iPhone3G() { return iPhone3G_; }
BOOL FlxG::iPhone3GS() { return iPhone3GS_; }
BOOL FlxG::iPodTouch1G() { return iPodTouch1G_; }
BOOL FlxG::iPodTouch2G() { return iPodTouch2G_; }
BOOL FlxG::iPodTouch3G() { return iPodTouch3G_; }
void FlxG::setMusic(FlxSound* m) { music_ = m; }
FlxSound* FlxG::music() { return music_; }
void FlxG::setQuake(FlxQuake* q) { quake_ = q; }
FlxQuake* FlxG::quake() { return quake_; }
void FlxG::setLevel(int l) { level_ = l; }
int FlxG::level() { return level_; }
void FlxG::setElapsed(float e) { elapsed_ = e; }
float FlxG::elapsed() { return elapsed_; }
void FlxG::setMaxElapsed(float e) { maxElapsed_ = e; }
float FlxG::maxElapsed() { return maxElapsed_; }
void FlxG::setScroll(FlashPoint* s) { scroll_ = s; }
FlashPoint* FlxG::scroll() { return scroll_; }
void FlxG::setScore(int s) { score_ = s; }
int FlxG::score() { return score_; }
void FlxG::setFlash(FlxFlash* f) { flash_ = f; }
FlxFlash* FlxG::flash() { return flash_; }
void FlxG::setFade(FlxFade* f) { fade_ = f; }
FlxFade* FlxG::fade() { return fade_; }
void FlxG::setHeight(int h) { height_ = h; }
int FlxG::height() { return height_; }
void FlxG::setWidth(int w) { width_ = w; }
int FlxG::width() { return width_; }
void FlxG::setTimeScale(float t) { timeScale_ = t; }
float FlxG::timeScale() { return timeScale_; }
FlxTouches* FlxG::touches() { return touches_; }
FlxGame* FlxG::game() { return _game; }
void FlxG::setPauseFollow(BOOL p) { pauseFollow_ = p; }
BOOL FlxG::pauseFollow() { return pauseFollow_; }

void FlxG::didEnterBackground() {}
void FlxG::willEnterForeground() {}
// + (void) willResignActive;
void FlxG::willResignActive() {
    if (audioPlayer && plat::musicPlaying()) plat::pauseMusic();
    if (_game) _game->setPaused(true);
}
// + (void) didBecomeActive;
void FlxG::didBecomeActive() {
    if (_game) _game->setPaused(false);
    if (audioPlayer && !musicPaused) plat::unpauseMusic();
}

// + (void) vibrate;   PORT: no vibration motor on desktop; the setting is still read.
void FlxG::vibrate() {
    if (!checkedVibration) {
        doVibration = UserDefaults::standard().getBool("Vibrate", false);
        checkedVibration = true;
    }
}

void FlxG::log(const std::string& Data) { SDL_Log("FlxG.log: %s", Data.c_str()); }

void FlxG::setState(FlxState* State) { _game->switchState(State); }
FlxState* FlxG::state() { return _game ? _game->_state : nullptr; }

// + (void) playMusicWithParam1:(NSString *)Music param2:(float)Volume;   (Volume is ignored, sic)
void FlxG::playMusic(const std::string& Music, float Volume) { flxPendingMusic = Music; }

// + (void) reallyPlayMusic:(NSString *)Music
void FlxG::reallyPlayMusic(const std::string& Music) {
    float volume = musicMasterVolume;
    if (volume > 0.0) {
        audioPlayer = plat::playMusic(Music, volume);
        if (!audioPlayer) SDL_Log("error!");
    }
}
// + (void) fadeOutMusic:(float)duration;
void FlxG::fadeOutMusic(float duration) {
    float volume = musicMasterVolume;
    fadeOutVolume = volume;
    fadeOutTimer = fadeOutDuration = duration;
}
// + (void) updateSounds;
void FlxG::updateSounds() {
    if (fixedDtMode()) gSoundClock += elapsed_;
    if (fadeOutTimer <= 0) return;
    fadeOutTimer -= elapsed_;
    if (fadeOutTimer < 0) fadeOutTimer = 0;
    plat::setMusicVolume(fadeOutVolume * (fadeOutTimer / fadeOutDuration));
    if (fadeOutTimer == 0) {
        plat::stopMusic();
        audioPlayer = false;
    }
}
// + (void) pauseMusic
void FlxG::pauseMusic() {
    if (audioPlayer) {
        plat::pauseMusic();
        musicPaused = true;
    }
}
// + (void) unpauseMusic
void FlxG::unpauseMusic() {
    if (audioPlayer && !plat::musicPlaying()) {
        plat::unpauseMusic();
        musicPaused = false;
    }
}

// + (void) loadSound:(NSString *)filename
void FlxG::loadSound(const std::string& filename) { plat::loadSound(filename); }

// + (FlxSound *) playWithParam1:(NSString *)EmbeddedSound param2:(float)Volume param3:(BOOL)Looped;

FlxSound* FlxG::play(const std::string& EmbeddedSound, float Volume, BOOL Looped) {
    if (!plat::loadSound(EmbeddedSound)) {
        SDL_Log("unable to load sound: %s", EmbeddedSound.c_str());
        return nullptr;
    }
    plat::playSound(EmbeddedSound, Volume * soundEffectsMasterVolume, Looped);
    if (fixedDtMode()) gSoundEnds[EmbeddedSound] = Looped ? 1e300 : gSoundClock + wavSeconds(EmbeddedSound);
    return nullptr;
}
// + (void) stop:(NSString *)EmbeddedSound;
void FlxG::stop(const std::string& EmbeddedSound) {
    plat::stopSound(EmbeddedSound);
    if (fixedDtMode()) gSoundEnds.erase(EmbeddedSound);
}
// + (BOOL) playing:(NSString *)EmbeddedSound
BOOL FlxG::playing(const std::string& EmbeddedSound) {
    if (fixedDtMode()) {
        auto it = gSoundEnds.find(EmbeddedSound);
        return it != gSoundEnds.end() && gSoundClock < it->second;
    }
    return plat::soundPlaying(EmbeddedSound);
}

// + (BOOL) checkBitmapCache:(NSString *)Key;
BOOL FlxG::checkBitmapCache(const std::string& Key) { return _cache.count(Key) && _cache[Key] != nullptr; }

// + (SemiSecretTexture *) addTextureWithParam1:(NSString *)Graphic param2:(BOOL)Unique;
SemiSecretTexture* FlxG::addTextureWithParam1(const std::string& Graphic, BOOL Unique) {
    std::string key = Graphic;
    if (Unique && _cache.count(key)) {
        unsigned int inc = 0;
        std::string ukey;
        do {
            ukey = key + std::to_string(inc++);
        } while (_cache.count(ukey));
        key = ukey;
    }
    auto it = _cache.find(key);
    SemiSecretTexture* texture = it != _cache.end() ? it->second : nullptr;
    if (texture == nullptr) {
        texture = SemiSecretTexture::textureWithImage(Graphic);
        if (texture) {
            _cache[key] = texture;  // [NSMutableDictionary setObject:nil] would throw
            // PORT: a unique texture belongs to the state that asked for it (freed with its zone)
            if (Unique && gFlxZone) gFlxZone->uniqueTextures.push_back(key);
        }
    }
    return texture;
}
void FlxG::removeTexture(const std::string& Key) {
    auto it = _cache.find(Key);
    if (it == _cache.end()) return;
    delete it->second;
    _cache.erase(it);
}
void FlxG::setTexture(SemiSecretTexture* Texture, const std::string& Key) { _cache[Key] = Texture; }

// + (SemiSecretTexture *) createBitmapWithParam1:param2:param3:param4:param5:
SemiSecretTexture* FlxG::createBitmapWithParam1(unsigned int Width, unsigned int Height, unsigned int Color, BOOL Unique, const std::string& Key) {
    std::string key = Key;
    if (key.empty()) {
        char b[64];
        snprintf(b, sizeof b, "%dx%d:%d", Width, Height, (int)Color);
        key = b;
        if (Unique && _cache.count(key)) {
            unsigned int inc = 0;
            std::string ukey;
            do {
                ukey = key + std::to_string(inc++);
            } while (_cache.count(ukey));
            key = ukey;
        }
    }
    SemiSecretTexture* texture = _cache.count(key) ? _cache[key] : nullptr;
    if (texture == nullptr) {
        texture = SemiSecretTexture::textureWithColor(Color);
        _cache[key] = texture;
        // PORT: see addTextureWithParam1 (only auto-generated unique keys; explicit keys may be shared)
        if (Unique && Key.empty() && gFlxZone) gFlxZone->uniqueTextures.push_back(key);
    }
    return texture;
}
void FlxG::clearTextureCache() { _cache.clear(); }

// + (void) followWithParam1:(FlxObject *)Target param2:(float)Lerp;
void FlxG::followWithParam1(FlxObject* Target, float Lerp) {
    pauseFollow_ = false;
    followTarget = Target;
    followLerp = Lerp;
    _scrollTarget->x = ((int)width_ >> 1) - followTarget->x - ((int)(followTarget->getWidth()) >> 1);
    _scrollTarget->y = ((int)height_ >> 1) - followTarget->y - ((int)(followTarget->getHeight()) >> 1);
    scroll_->x = _scrollTarget->x;
    scroll_->y = _scrollTarget->y;
    doFollow();
}
// + (void) followAdjustWithParam1:(float)LeadX param2:(float)LeadY;
void FlxG::followAdjustWithParam1(float LeadX, float LeadY) { followLead = (new FlashPoint)->initWithX(LeadX, LeadY); }
// + (void) followBoundsWithParam1:param2:param3:param4:param5:
void FlxG::followBoundsWithParam1(int MinX, int MinY, int MaxX, int MaxY, BOOL UpdateWorldBounds) {
    followMin = (new FlashPoint)->initWithX(-MinX, -MinY);
    followMax = (new FlashPoint)->initWithX(-MaxX + width_, -MaxY + height_);
    if (followMax->x > followMin->x) followMax->x = followMin->x;
    if (followMax->y > followMin->y) followMax->y = followMin->y;
    if (UpdateWorldBounds) FlxU::setWorldBoundsWithParam1(-MinX, -MinY, -MinX + MaxX, -MinY + MaxY);
    doFollow();
}

// + (void) setGameData:(FlxGame *)Game width:(int)Width height:(int)Height zoom:(float)Zoom;
void FlxG::setGameData(FlxGame* Game, int Width, int Height, float Zoom) {
    _game = Game;
    width_ = Width;
    height_ = Height;
    scroll_ = nullptr;
    _scrollTarget = nullptr;
    unfollow();
    level_ = 0;
    score_ = 0;
    FlxU::setSeed(NAN);
    quake_ = (new FlxQuake)->initWithZoom(Zoom);
    flash_ = (new FlxFlash)->init();
    fade_ = (new FlxFade)->init();
    FlxU::setWorldBoundsWithParam1();
}

// + (void) doFollow;
void FlxG::doFollow() {
    if (!pauseFollow_) {
        if (followTarget != nullptr) {
            _scrollTarget->x = ((int)width_ >> 1) - followTarget->x - ((int)(followTarget->getWidth()) >> 1);
            _scrollTarget->y = ((int)height_ >> 1) - followTarget->y - ((int)(followTarget->getHeight()) >> 1);
            if ((followLead != nullptr) && dynamic_cast<FlxSprite*>(followTarget)) {
                _scrollTarget->x -= followTarget->velocity.x * followLead->x;
                _scrollTarget->y -= followTarget->velocity.y * followLead->y;
            }
            scroll_->x += (_scrollTarget->x - scroll_->x) * followLerp * elapsed_;
            scroll_->y += (_scrollTarget->y - scroll_->y) * followLerp * elapsed_;
            if (followMin != nullptr) {
                if (scroll_->x > followMin->x) scroll_->x = followMin->x;
                if (scroll_->y > followMin->y) scroll_->y = followMin->y;
            }
            if (followMax != nullptr) {
                if (scroll_->x < followMax->x) scroll_->x = followMax->x;
                if (scroll_->y < followMax->y) scroll_->y = followMax->y;
            }
        }
    }
}

// + (void) unfollow;
void FlxG::unfollow() {
    followTarget = nullptr;
    followLead = nullptr;
    followLerp = 1;
    followMin = nullptr;
    followMax = nullptr;
    if (scroll_ == nullptr)
        scroll_ = (new FlashPoint)->initWithX();
    else
        scroll_->x = scroll_->y = 0;
    if (_scrollTarget == nullptr)
        _scrollTarget = (new FlashPoint)->initWithX();
    else
        _scrollTarget->x = _scrollTarget->y = 0;
}
