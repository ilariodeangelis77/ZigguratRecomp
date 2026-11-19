// TutorialState — 8 tutorial slides with bouncing arrows and an OK button.
// Reconstructed from the ZiGGURAT 1.0.1 binary (armv7), see re/decomp_softfp/game/TutorialState/.
#include "game/MenuCommon.h"

using namespace zig;

namespace {

class TutorialState : public FlxState {
public:
    FlxSprite *_arrowPrev = nullptr, *_arrowNext = nullptr;
    FlxText* _okButton = nullptr;
    FlxSprite *_aiming = nullptr, *_dontTouch = nullptr, *_howToPlay = nullptr, *_slideHoldRelease = nullptr,
              *_slingshot = nullptr, *_touchHere = nullptr, *_youCanView = nullptr, *_holdToCharge = nullptr;
    FlxText *_slide1PrecisionMode = nullptr, *_slide1HowToPlay = nullptr, *_slide2PrecisionMode = nullptr,
            *_slide2TouchHere1 = nullptr, *_slide2TouchHere2 = nullptr, *_slide3PrecisionMode = nullptr,
            *_slide3DontTouchHere = nullptr, *_slide4PrecisionMode = nullptr, *_slide4SlideToAim1 = nullptr,
            *_slide4SlideToAim2 = nullptr, *_slide5PrecisionMode = nullptr, *_slide5HoldToCharge = nullptr,
            *_slide5ReleaseToFire = nullptr, *_slide6ShootTheEyes = nullptr, *_slide7SlingshotMode = nullptr,
            *_slide8PrecisionMode = nullptr, *_slide8Options = nullptr;
    FlxGroup* _slides = nullptr;
    int _currentSlide = 0;

    FlxText* slideText(unsigned int width, txt::Id key, float size) {
        FlxText* t = newText(width, L(key), size);
        t->setAlignment("left");
        t->setX(1000.0f);
        t->setY(0);
        add(t);
        return t;
    }

    // -[TutorialState create]  ea=00057bd4
    void create() override {
        _currentSlide = 0;
        _aiming = (new FlxSprite)->init();
        _aiming->loadGraphic("aimsheet.png", true, false, 512, 384, true);
        _aiming->addAnimation("cycle", {0, 1, 2, 3, 4, 3, 2, 1}, 1.3f);
        _aiming->play("cycle");
        _holdToCharge = (new FlxSprite)->init();
        _holdToCharge->loadGraphic("holdtochargesheet.png", true, false, 512, 384, true);
        _holdToCharge->addAnimation("cycle", {0, 1, 2, 3, 3}, 1.6f);
        _holdToCharge->play("cycle");
        _dontTouch = newSprite("donttouch.png");
        _howToPlay = newSprite("black.png");
        _slideHoldRelease = newSprite("black.png");
        _slingshot = newSprite("black.png");
        _touchHere = newSprite("touchhere.png");
        _youCanView = newSprite("black.png");

        _slides = (new FlxGroup)->init();
        _slides->setX(0);
        _slides->setY(0);
        add(_slides);
        for (FlxSprite* s : {_howToPlay, _touchHere, _dontTouch, _aiming, _holdToCharge, _slideHoldRelease, _slingshot, _youCanView})
            _slides->add(s);
        for (size_t i = 0; i < _slides->members.size(); i++) {
            auto* m = static_cast<FlxSprite*>(_slides->members[i]);
            m->setY((float)FlxG::height() - m->getHeight());
            m->setX(m->frameWidth * (float)(unsigned int)i);
        }

        const unsigned int W = (unsigned int)FlxG::width();
        _slide1PrecisionMode = slideText(W, txt::kOptPrecision, 28.0f);
        _slide1HowToPlay = slideText(W, txt::kOptHowToPlay, 18.0f);
        _slide2PrecisionMode = slideText(W, txt::kOptPrecision, 28.0f);
        const float touchHereSize = lang("de") ? 15.0f : 18.0f;  // DAT_00058da4 / DAT_00058da8
        _slide2TouchHere1 = slideText(W, txt::kTutTouchHere, touchHereSize);
        _slide2TouchHere2 = slideText(W, txt::kTutTouchHere, touchHereSize);
        _slide3PrecisionMode = slideText(W, txt::kOptPrecision, 28.0f);
        _slide3DontTouchHere = slideText(W, txt::kTutDontTouch, 18.0f);
        _slide4PrecisionMode = slideText(W, txt::kOptPrecision, 28.0f);
        float aimSize;
        if (lang("fr")) aimSize = 15.0f;
        else if (lang("de") || lang("es") || lang("pt") || lang("pt-PT") || lang("pt-BR")) aimSize = 12.0f;
        else if (lang("it")) aimSize = 10.0f;
        else aimSize = 18.0f;
        _slide4SlideToAim1 = slideText(200, txt::kTutSlideToAim, aimSize);
        _slide4SlideToAim2 = slideText(200, txt::kTutSlideToAim, aimSize);
        _slide5PrecisionMode = slideText(W, txt::kOptPrecision, 28.0f);
        float holdSize;
        if (lang("de") || lang("pt") || lang("pt-PT")) holdSize = 14.0f;
        else if (lang("it")) holdSize = 12.0f;
        else holdSize = 18.0f;
        _slide5HoldToCharge = slideText(W, txt::kTutHoldToCharge, holdSize);
        _slide5ReleaseToFire = slideText(W, txt::kTutReleaseToFire, lang("de") ? 15.0f : 18.0f);  // DAT_00059958
        _slide6ShootTheEyes = slideText(W, txt::kTutShootEyes, 18.0f);
        _slide7SlingshotMode = slideText(436, txt::kTutSlingshotHint, 18.0f);
        _slide8PrecisionMode = slideText(W, txt::kOptPrecision, 28.0f);
        _slide8Options = slideText(400, txt::kTutViewAgain, 18.0f);

        _arrowPrev = (new FlxSprite)->init();
        _arrowPrev->loadGraphic("arrowSheet.png", true, true, 69, 42);
        _arrowPrev->setFacing(LEFT);
        _arrowPrev->addAnimation("bounce", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, 5.0f, true);
        _arrowPrev->play("bounce");
        _arrowPrev->setX(0);
        _arrowPrev->setY(halfH() - _arrowPrev->frameHeight * 0.5f);
        _arrowPrev->visible = false;
        add(_arrowPrev);

        _arrowNext = (new FlxSprite)->init();
        _arrowNext->loadGraphic("arrowSheet.png", true, true, 69, 42);
        _arrowNext->setFacing(RIGHT);
        _arrowNext->addAnimation("bounce", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}, 5.0f, true);
        _arrowNext->play("bounce");
        _arrowNext->setX((float)FlxG::width() - _arrowNext->getWidth());
        _arrowNext->setY(halfH() - _arrowNext->frameHeight * 0.5f);
        add(_arrowNext);

        _okButton = newText((unsigned int)(_arrowNext->getWidth() + 50.0f), L(txt::kTutOk), 30.0f);  // DAT_00059960
        _okButton->setAlignment("right");
        _okButton->setColor(0xff0066);
        _okButton->exists = false;
        _okButton->setX(_arrowNext->x + -44.0f);  // DAT_00059964
        _okButton->setY(_arrowNext->y + _arrowNext->getHeight() / 5.0f);
        add(_okButton);
    }

    // -[TutorialState update]  ea=00059968
    void update() override {
        if (FlxG::touches()->touchesBegan) {
            if (!touchOver(_arrowPrev)) {
                if (touchOver(_arrowNext)) {
                    _arrowPrev->visible = true;
                    _currentSlide = _currentSlide + 1;
                    if (_currentSlide == 7) {
                        _arrowNext->visible = false;
                        _okButton->exists = true;
                    }
                    if (_currentSlide == 8) {
                        UserDefaults& d = UserDefaults::standard();
                        if (!d.has("firstStartup")) {
                            d.set("firstStartup", "no");
                            d.set("preferredControlScheme", "precision");
                            d.set("aimGuide", "no");
                            d.synchronize();
                            FlxG::pauseMusic();
                            FlxG::setState(makeStartState());
                        } else {
                            FlxG::playMusic("Overture.mp3");
                            FlxG::setState(makeOptionsState());
                        }
                    } else {
                        FlxG::play("lightningStrike.wav", 0.3f);
                    }
                }
            } else {
                if (0 < _currentSlide) FlxG::play("lightningStrike.wav", 0.3f);
                _arrowNext->visible = true;
                _okButton->exists = false;
                _currentSlide = _currentSlide - 1;
                if (_currentSlide < 1) {
                    _currentSlide = 0;
                    _arrowPrev->visible = false;
                }
            }
        }
        // ease the slide strip towards the current slide: x += (x - target) * -5 * elapsed
        const float target = (float)(FlxG::width() / 2 + _currentSlide * -512 + -256);
        if (_slides->x > target || _slides->x < target) {
            float d = (_slides->x - target) * -5.0f;
            d = d * FlxG::elapsed();
            _slides->setX(_slides->x + d);
        }
        if (_currentSlide < 8) updateTextPositions();
        FlxState::update();
    }

    FlxObject* slide(int i) { return _slides->members[i]; }

    // -[TutorialState updateTextPositions]  ea=00059f5c
    void updateTextPositions() {
        FlxObject* m = slide(0);
        _slide1PrecisionMode->setX(m->x + 116.0f);
        _slide1PrecisionMode->setY(m->y + 82.0f);
        _slide1HowToPlay->setX(m->x + 180.0f);
        _slide1HowToPlay->setY(m->y + 128.0f);
        m = slide(1);
        _slide2PrecisionMode->setX(m->x + 34.0f);
        _slide2PrecisionMode->setY(m->y + 82.0f);
        _slide2TouchHere1->setX(m->x + 57.0f);
        _slide2TouchHere1->setY(m->y + 327.0f);
        _slide2TouchHere2->setX(m->x + 314.0f);
        _slide2TouchHere2->setY(m->y + 327.0f);
        m = slide(2);
        _slide3PrecisionMode->setX(m->x + 34.0f);
        _slide3PrecisionMode->setY(m->y + 82.0f);
        _slide3DontTouchHere->setX(m->x + 148.0f);
        _slide3DontTouchHere->setY(m->y + 148.0f);
        m = slide(3);
        _slide4PrecisionMode->setX(m->x + 34.0f);
        _slide4PrecisionMode->setY(m->y + 82.0f);
        _slide4SlideToAim1->setX(m->x + 72.0f);
        _slide4SlideToAim1->setY(m->y + 327.0f);
        _slide4SlideToAim2->setX(m->x + 292.0f);
        _slide4SlideToAim2->setY(m->y + 327.0f);
        // which slide-to-aim label shows depends on the aim animation frame ([FlxSprite currentFrame])
        switch (static_cast<FlxSprite*>(m)->_curFrame) {
            case 0: case 1: case 7: _slide4SlideToAim1->exists = false; _slide4SlideToAim2->exists = true; break;
            case 2: case 6: _slide4SlideToAim1->exists = true; _slide4SlideToAim2->exists = true; break;
            case 3: case 4: case 5: _slide4SlideToAim1->exists = true; _slide4SlideToAim2->exists = false; break;
            default: break;
        }
        m = slide(4);
        _slide5PrecisionMode->setX(m->x + 34.0f);
        _slide5PrecisionMode->setY(m->y + 82.0f);
        _slide5HoldToCharge->setX(m->x + 114.0f);
        _slide5HoldToCharge->setY(m->y + 327.0f);
        _slide5ReleaseToFire->setX(m->x + 114.0f);
        _slide5ReleaseToFire->setY(m->y + 327.0f);
        if (lang("de")) _slide5HoldToCharge->setX(_slide5HoldToCharge->x + -60.0f);
        unsigned int f = static_cast<FlxSprite*>(m)->_curFrame;
        if (f == 3) { _slide5HoldToCharge->exists = false; _slide5ReleaseToFire->exists = true; }
        else if (f == 0) { _slide5HoldToCharge->exists = true; _slide5ReleaseToFire->exists = false; }
        m = slide(5);
        _slide6ShootTheEyes->setX(m->x + 158.0f);
        _slide6ShootTheEyes->setY(m->y + 160.0f);
        m = slide(6);
        _slide7SlingshotMode->setX(m->x + 38.0f);
        _slide7SlingshotMode->setY(m->y + 126.0f);
        m = slide(7);
        _slide8PrecisionMode->setX(m->x + 116.0f);
        _slide8PrecisionMode->setY(m->y + 82.0f);
        _slide8Options->setX(m->x + 58.0f);
        _slide8Options->setY(m->y + 136.0f);
    }
};

}  // namespace

FlxState* makeTutorialState() { return (new TutorialState)->init(); }
