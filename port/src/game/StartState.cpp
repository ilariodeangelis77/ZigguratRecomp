// StartState — logo sequence (Action Button / Flixel + Freshuu), title, options button.
// Reconstructed from the ZiGGURAT 1.0.1 binary (armv7), see re/decomp_softfp/game/StartState/.
#include <SDL3/SDL.h>

#include "game/Enhancements.h"
#include "game/MenuCommon.h"

using namespace zig;

namespace {

class StartState : public FlxState {
public:
    float _stateTime = 0;
    int _state = 0;
    FlxSprite *_sky = nullptr, *_stars = nullptr, *_moon = nullptr, *_cloud4 = nullptr, *_cloud3 = nullptr,
              *_cloud2 = nullptr, *_mainCloud = nullptr, *_actionButton = nullptr, *_flixelLogo = nullptr,
              *_freshuuLogo = nullptr, *_logo = nullptr, *_screenFill = nullptr, *_silhouette = nullptr,
              *_optionsButton = nullptr;
    bool _godMode = false;

    // -[StartState create]  ea=0004cc6c
    void create() override {
        initOpenfeint();
        initData();
        if (!checkForFirstStartup()) FlxG::playMusic("Overture.mp3");
        const float half = 0.5f;

        _sky = newSprite("sky_night.png");
        _sky->setX(halfW() - _sky->getWidth() * half);
        _sky->setY(halfH() - _sky->getHeight() * half);
        add(_sky);

        _cloud4 = newSprite("clouds4_night.png");
        _cloud4->setX(0);
        _cloud4->setY((float)(FlxG::height() - 128));
        add(_cloud4);

        _cloud3 = newSprite("clouds3_night.png");
        _cloud3->setX(0);
        _cloud3->setY((float)(FlxG::height() - 115));
        add(_cloud3);

        _logo = newSprite("ziglogo.png");
        _logo->setX(halfW() - _logo->getWidth() * half);
        _logo->setY(halfH() - _logo->getHeight() * half + 150.0f);  // DAT_0004d45c
        _logo->setAlpha(0);
        add(_logo);

        _cloud2 = newSprite("clouds2_night.png");
        _cloud2->setX(0);
        _cloud2->setY((float)(FlxG::height() - 84));
        add(_cloud2);

        _stars = newSprite("stars.png");
        _stars->setX(halfW() - _stars->getWidth() * half);
        _stars->setY((float)FlxG::height() - _stars->getHeight());
        add(_stars);

        _moon = newSprite("moon.png");
        _moon->setX((float)(FlxG::width() - 180));
        _moon->setY((float)(FlxG::height() - 315));
        add(_moon);

        _mainCloud = newSprite("mainCloudsNight.png");
        _mainCloud->setX(0);
        _mainCloud->setY((float)(FlxG::height() - 38));
        add(_mainCloud);

        _screenFill = (new FlxSprite)->initWithXY(0, 0);
        _screenFill->createGraphicWithParam1(FlxG::width(), FlxG::height(), 0, true);
        _screenFill->setAlpha(1.0f);
        add(_screenFill);

        _actionButton = newSprite("actionbutton.png");
        _actionButton->setX(halfW() - _actionButton->getWidth() * half);
        _actionButton->setY(halfH() - _actionButton->getHeight() * half);
        add(_actionButton);

        _flixelLogo = newSprite("flixel.png");
        _flixelLogo->setX(halfW() - _flixelLogo->getWidth() * half + 100.0f);  // DAT_0004d460
        _flixelLogo->setY(halfH() - _flixelLogo->getHeight() * half);
        _flixelLogo->setAlpha(0);
        add(_flixelLogo);

        _freshuuLogo = newSprite("freshuu.png");
        _freshuuLogo->setX(halfW() - _freshuuLogo->getWidth() * half + -100.0f);  // DAT_0004daec
        _freshuuLogo->setY(halfH() - _freshuuLogo->getHeight() * half);
        _freshuuLogo->setAlpha(0);
        add(_freshuuLogo);

        _optionsButton = FlxSprite::spriteWithGraphic("img_options.png");
        _optionsButton->setX((float)FlxG::width() - _optionsButton->getWidth() + -10.0f);
        _optionsButton->setY((float)FlxG::height() + _optionsButton->getHeight());  // off-screen, slides in
        add(_optionsButton);

        _silhouette = newSprite("img_silhouette.png");
        _silhouette->setX(halfW() - _silhouette->getWidth() * half);
        _silhouette->setY((float)FlxG::height() - _silhouette->getHeight());
        _silhouette->exists = false;
        add(_silhouette);

        _stateTime = 0;
        _state = 0;
    }

    // Skip the logo sequence (tap during states 0..7).
    void skipToTitle(bool fadeActionButton) {
        _screenFill->setAlpha(0);
        _flixelLogo->setAlpha(0);
        _freshuuLogo->setAlpha(0);
        if (fadeActionButton) _actionButton->setAlpha(0);
        _stateTime = 0;
        _state = 8;
        _logo->setY(halfH() + _logo->getHeight() * -0.5f);
        _logo->setAlpha(1.0f);
    }

    // -[StartState update]  ea=0004daf0
    void update() override {
        // logos slowly grow: scale += elapsed * 0.025
        const float g = 0.025f;  // DAT_0004deb8
        _flixelLogo->scale = CGPointMake(_flixelLogo->scale.x + FlxG::elapsed() * g, _flixelLogo->scale.y + FlxG::elapsed() * g);
        _freshuuLogo->scale = CGPointMake(_freshuuLogo->scale.x + FlxG::elapsed() * g, _freshuuLogo->scale.y + FlxG::elapsed() * g);
        _actionButton->scale = CGPointMake(_actionButton->scale.x + FlxG::elapsed() * g, _actionButton->scale.y + FlxG::elapsed() * g);
        _stateTime = _stateTime + FlxG::elapsed();
        const bool began = FlxG::touches()->touchesBegan;
        // ENHANCEMENT skipLogos: behave as if the player tapped through the logo sequence immediately.
        if (enh().skipLogos && _state < 8) skipToTitle(true);

        switch (_state) {
            case 0:  // Action Button logo shown for 4.5 s
                if (1.0f <= _stateTime / 4.5f) { _stateTime = 0; _state = 1; }
                else if (began) skipToTitle(true);
                break;
            case 1:  // Action Button logo fades out over 2.5 s
                _actionButton->setAlpha(1.0f - _stateTime / 2.5f);
                if (1.0f <= _stateTime / 2.5f) { _actionButton->setAlpha(0); _state = 2; _stateTime = 0; }
                else if (began) skipToTitle(true);
                break;
            case 2:  // 1 s of black
                if (_stateTime < 1.0f) {
                    if (began) skipToTitle(false);
                } else {
                    _stateTime = 0;
                    _state = 3;
                    _flixelLogo->scale = CGPointMake(0.5f, 0.5f);
                    _freshuuLogo->scale = CGPointMake(1.0f, 1.0f);
                }
                break;
            case 3: {  // flixel + freshuu fade in over 3 s
                float a = _stateTime / 3.0f;
                _flixelLogo->setAlpha(a);
                _freshuuLogo->setAlpha(a);
                if (1.0f <= a) { _flixelLogo->setAlpha(1); _freshuuLogo->setAlpha(1); _stateTime = 0; _state = 4; }
                if (began) skipToTitle(false);
                break;
            }
            case 4:  // hold 1 s
                if (1.0f <= _stateTime) { _stateTime = 0; _state = 5; }
                if (began) skipToTitle(false);
                break;
            case 5: {  // fade out over 2 s
                float t = _stateTime * 0.5f;
                _flixelLogo->setAlpha(1.0f - t);
                _freshuuLogo->setAlpha(1.0f - t);
                if (1.0f <= t) { _flixelLogo->setAlpha(0); _freshuuLogo->setAlpha(0); _stateTime = 0; _state = 6; }
                if (began) skipToTitle(false);
                break;
            }
            case 6:
                if (_stateTime < 1.0f && !began) break;
                _stateTime = 0;
                _state = 7;
                break;
            case 7: {  // black fill fades out over 2 s, revealing the night sky
                float t = _stateTime * 0.5f;
                _screenFill->setAlpha(1.0f - t);
                if (1.0f <= t) { _screenFill->setAlpha(0); _stateTime = 0; _state = 8; }
                if (began) skipToTitle(false);
                break;
            }
            case 8: {  // title: logo rises, then the options button slides in
                const float target = halfH() + _logo->getHeight() * -0.5f;
                if (_logo->y <= target) {
                    if ((float)FlxG::height() - _optionsButton->getHeight() < _optionsButton->y) {
                        float d = FlxG::elapsed() * -120.0f;  // DAT_0004ec0c
                        _optionsButton->setY(_optionsButton->y + d);
                    }
                } else {
                    float d = FlxG::elapsed() * -70.0f;  // DAT_0004ec08
                    _logo->setY(_logo->y + d);
                }
                if (_logo->alpha() < 1.0f) {
                    double d = (double)FlxG::elapsed() * 0.4;  // DAT_0004ec10 (double)
                    _logo->setAlpha((float)((double)_logo->alpha() + d));
                }
                if (began) {
                    if (!touchOver(_optionsButton)) {
                        if (halfH() + _logo->getHeight() * -0.5f < _logo->y) {
                            // first tap only finishes the logo animation
                            _logo->setY(halfH() + _logo->getHeight() * -0.5f);
                            _logo->setAlpha(1.0f);
                        } else {
                            FlxG::pauseMusic();
                            FlxG::play("gameOver.wav");
                            _stateTime = 0;
                            _state = 9;
                            _silhouette->exists = true;
                            _godMode = false;
                        }
                    } else {
                        FlxG::play("shoot.wav");
                        FlxG::setState(makeOptionsState());
                    }
                }
                break;
            }
            case 9:
                FlxG::setState(playStateFromDefaults(_godMode));
                return;  // original jumps straight to setState: no cloud scroll this frame
        }
        scrollClouds(_mainCloud, _cloud2, _cloud3, _cloud4);
        // NB: the original never calls [super update] here (no FlxState update of members).
    }

    // -[StartState initOpenfeint]
    // PORT: OpenFeint (and its Game Center bridge) is long dead; nothing to initialise.
    void initOpenfeint() {}

    // -[StartState initData]: fetch Statistics, insert a zeroed record if none, save.
    void initData() {
        Statistics::shared();  // load or insert-if-missing
        Statistics::shared().save();
    }

    // -[StartState checkForFirstStartup]
    bool checkForFirstStartup() {
        if (!UserDefaults::standard().has("firstStartup")) {
            FlxG::pauseMusic();
            FlxG::setState(makeTutorialState());
            return true;
        }
        return false;
    }

    // -[StartState resetStats] / -deleteAllObjects: (debug helpers, unreferenced at runtime)
    void resetStats() { Statistics::shared().resetAll(); }

    // -[StartState consoleOutput]: NSLog dump of the stats record.
    void consoleOutput() {
        SDL_Log("StartState stats: games %d kills %d", Statistics::shared().getInt("gamesPlayed"),
                Statistics::shared().getInt("kills"));
    }
};

}  // namespace

FlxState* makeStartState() { return (new StartState)->init(); }
