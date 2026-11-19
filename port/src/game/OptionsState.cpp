// OptionsState — control scheme, aiming guide, how to play, stats, play now.
// Reconstructed from the ZiGGURAT 1.0.1 binary (armv7), see re/decomp_softfp/game/OptionsState/.
#include "game/Enhancements.h"
#include "game/MenuCommon.h"

using namespace zig;

namespace {

class OptionsState : public FlxState {
public:
    FlxSprite *_sky = nullptr, *_cloud4 = nullptr, *_cloud3 = nullptr, *_cloud2 = nullptr, *_mainCloud = nullptr,
              *_pillar = nullptr;
    FlxText *_options = nullptr, *_precisionMode = nullptr, *_slingshotMode = nullptr, *_aimingMode = nullptr,
            *_aimModeOnOff = nullptr, *_howToPlay = nullptr, *_viewStats = nullptr, *_facebook = nullptr,
            *_twitter = nullptr, *_playNow = nullptr;
    FlxSprite *_OFbutton = nullptr, *_GCbutton = nullptr, *_schemeSelector = nullptr, *_howToPlayBox = nullptr,
              *_viewStatsBox = nullptr, *_silhouette = nullptr, *_screenDarken = nullptr;
    int _selectedScheme = 0;
    bool _aimingGuideActivated = false;
    float _silhouetteTimer = 0;
    float _tweetTime = 0;
    FlxSprite *_altTextOverlay = nullptr, *_altTextOn = nullptr, *_altTextOff = nullptr;

    FlxText* label(unsigned int width, txt::Id key, float size, const char* align, float x, float y) {
        FlxText* t = newText(width, L(key), size);
        t->setAlignment(align);
        t->setX(x);
        t->setY(y);
        return t;
    }

    FlxSprite* fullWidthImage(const std::string& g) {
        FlxSprite* s = newSprite(g);
        s->setX(halfW() - s->getWidth() * 0.5f);
        s->setY((float)FlxG::height() - s->getHeight());
        return s;
    }

    // -[OptionsState create]  ea=0005aa1c
    void create() override {
        getUpdateStrings();
        _selectedScheme = 0;
        _aimingGuideActivated = false;
        if (prefEqualsOrNil("preferredControlScheme", "slingshot")) _selectedScheme = 1;
        if (prefEqualsOrNil("aimGuide", "on")) _aimingGuideActivated = true;
        const float H = (float)FlxG::height();

        _sky = newSprite("sky_day.png");
        _sky->setX(halfW() - _sky->getWidth() * 0.5f);
        _sky->setY(halfH() - _sky->getHeight() * 0.5f);
        add(_sky);
        _cloud4 = newSprite("clouds4_day.png");
        _cloud4->setX(0);
        _cloud4->setY((float)(FlxG::height() - 128));
        add(_cloud4);
        _cloud3 = newSprite("clouds3_day.png");
        _cloud3->setX(0);
        _cloud3->setY((float)(FlxG::height() - 115));
        add(_cloud3);
        _cloud2 = newSprite("clouds2_day.png");
        _cloud2->setX(0);
        _cloud2->setY((float)(FlxG::height() - 84));
        add(_cloud2);
        _pillar = fullWidthImage("img_pillar.png");
        add(_pillar);
        _mainCloud = newSprite("mainClouds.png");
        _mainCloud->setX(0);
        _mainCloud->setY((float)(FlxG::height() - 38));
        add(_mainCloud);

        _options = label(180, txt::kOptTitle, 20.0f, "center", (float)(FlxG::width() / 2 - 90), (float)(FlxG::height() - 304));
        add(_options);

        const float schemeSize = lang("it") ? 12.0f : 14.0f;  // DAT_0005bf50 / DAT_0005bf54
        _precisionMode = label(180, txt::kOptPrecision, schemeSize, "center", (float)(FlxG::width() / 2 - 200), (float)(FlxG::height() - 250));
        _slingshotMode = label(180, txt::kOptSlingshot, schemeSize, "center", (float)(FlxG::width() / 2 + 20), (float)(FlxG::height() - 250));
        _schemeSelector = (new FlxSprite)->init();
        _schemeSelector->createGraphicWithWidth(180, (unsigned int)_precisionMode->getHeight(), 0xff0066);
        _schemeSelector->setX(_selectedScheme == 0 ? _precisionMode->x : _slingshotMode->x);
        _schemeSelector->setY(_precisionMode->y);
        add(_schemeSelector);
        add(_precisionMode);
        add(_slingshotMode);

        float aimSize;
        if (lang("it")) aimSize = 12.0f;
        else if (lang("pt") || lang("pt-PT")) aimSize = 10.0f;
        else aimSize = 14.0f;
        _aimingMode = label(180, txt::kOptAimGuide, aimSize, "center", (float)(FlxG::width() / 2 - 100), (float)(FlxG::height() - 214));
        add(_aimingMode);

        // Japanese alt-text images. The binary evaluates [lang isEqualToString:@"ja"] but discards the
        // result: these sprites are created and toggled but never added to the state, so they never draw.
        _altTextOverlay = fullWidthImage("img_jaOptionsOverlay.png");
        _altTextOff = fullWidthImage("img_jaOptionsOff.png");
        _altTextOn = fullWidthImage("img_jaOptionsOn.png");
        unsigned int onOffColor;
        std::string onOff;
        if (!_aimingGuideActivated) {
            onOff = L(txt::kOptOff);
            _altTextOff->exists = true;
            onOffColor = 0xff0066;
        } else {
            onOff = L(txt::kOptOn);
            _altTextOff->exists = false;
            onOffColor = 0x33ff66;
        }
        _altTextOn->exists = _aimingGuideActivated;
        _aimModeOnOff = newText(180, onOff, aimSize);
        _aimModeOnOff->setAlignment("left");
        _aimModeOnOff->setX((float)(FlxG::width() / 2 + 60));
        _aimModeOnOff->setY((float)(FlxG::height() - 214));
        _aimModeOnOff->setColor(onOffColor);
        add(_aimModeOnOff);
        // ENHANCEMENT showJaOverlay: actually add the Japanese alt-text images the original builds but drops.
        if (enh().showJaOverlay && preferredLanguage() == "ja") {
            add(_altTextOverlay);
            add(_altTextOff);
            add(_altTextOn);
        }

        _howToPlayBox = (new FlxSprite)->init();
        _howToPlayBox->createGraphicWithWidth(133, 32, 0xff0066);
        _howToPlayBox->setX((float)(FlxG::width() / 2 - 174));
        _howToPlayBox->setY((float)(FlxG::height() - 144));
        _howToPlayBox->exists = false;
        add(_howToPlayBox);
        float howSize;
        if (lang("fr")) howSize = 15.0f;
        else if (lang("it")) howSize = 14.0f;
        else howSize = 16.0f;
        _howToPlay = label(180, txt::kOptHowToPlay, howSize, "center", (float)(FlxG::width() / 2 - 196), (float)(FlxG::height() - 140));
        add(_howToPlay);

        _viewStatsBox = (new FlxSprite)->init();
        _viewStatsBox->createGraphicWithWidth(133, 32, 0xff0066);
        _viewStatsBox->setX((float)(FlxG::width() / 2 + 42));
        _viewStatsBox->setY((float)(FlxG::height() - 144));
        _viewStatsBox->exists = false;
        add(_viewStatsBox);
        const float statsSize = (lang("pt") || lang("pt-PT")) ? 11.0f : 16.0f;
        _viewStats = label(180, txt::kOptViewStats, statsSize, "center", (float)(FlxG::width() / 2 + 18), (float)(FlxG::height() - 140));
        add(_viewStats);

        _facebook = label(180, txt::kOptFacebook, 14.0f, "center", (float)(FlxG::width() / 2 - 260), (float)(FlxG::height() - 60));
        _facebook->setColor(0);
        add(_facebook);
        _twitter = label(180, txt::kOptTwitter, 14.0f, "center", (float)(FlxG::width() / 2 + 80), (float)(FlxG::height() - 60));
        add(_twitter);

        float playSize;
        if (lang("pt") || lang("pt-PT")) playSize = 24.0f;
        else if (lang("it")) playSize = 28.0f;
        else playSize = 34.0f;  // DAT_0005c314
        _playNow = label(220, txt::kOptPlayNow, playSize, "center", (float)(FlxG::width() / 2 - 106), (float)(FlxG::height() - 80));
        add(_playNow);

        // PORT: OpenFeint / Game Center icons kept for layout fidelity; tapping them only plays the click.
        _OFbutton = FlxSprite::spriteWithGraphic("img_OFlogo.png");
        _OFbutton->setX((float)FlxG::width() - _OFbutton->getWidth() + -5.0f);
        _OFbutton->setY((float)(FlxG::height() - 164));
        add(_OFbutton);
        _GCbutton = FlxSprite::spriteWithGraphic("img_GClogo.png");
        _GCbutton->setX((float)FlxG::width() - _GCbutton->getWidth() + -5.0f);
        _GCbutton->setY((float)(FlxG::height() - 130));
        add(_GCbutton);

        _silhouette = fullWidthImage("img_silhouette.png");
        _silhouette->exists = false;
        add(_silhouette);

        _screenDarken = (new FlxSprite)->initWithXY(0, 0);
        _screenDarken->createGraphicWithParam1(FlxG::width(), FlxG::height(), 0, true);
        _screenDarken->setAlpha(0);
        add(_screenDarken);
        // PORT: the tweet/facebook confirm dialog (confirm1-3, twCancel/twLogout/twConfirm, fbLogout/fbConfirm)
        // only appeared when ShareKit/Facebook reported a logged-in session, which can no longer happen.
        _silhouetteTimer = 0;
        (void)H;
    }

    // -[OptionsState update]  ea=0005d44c
    void update() override {
        if (!_silhouette->exists) {
            FlxText* target = _selectedScheme == 0 ? _precisionMode : _slingshotMode;
            const float tx = (float)(int)target->x;
            if (_schemeSelector->x > tx || _schemeSelector->x < tx) {
                float d = (_schemeSelector->x - tx) * -8.0f;
                d = d * FlxG::elapsed();
                _schemeSelector->setX(_schemeSelector->x + d);
            }
            if (FlxG::touches()->touchesBegan) {
                if (touchOver(_precisionMode)) {
                    if (_selectedScheme != 0) {
                        FlxG::play("enemyShoot.wav");
                        _selectedScheme = 0;
                    }
                } else if (touchOver(_slingshotMode)) {
                    if (_selectedScheme != 1) {
                        FlxG::play("enemyShoot.wav");
                        _selectedScheme = 1;
                    }
                } else if (touchOver(_aimingMode) || touchOver(_aimModeOnOff)) {
                    FlxG::play("shoot.wav");
                    _aimingGuideActivated = !_aimingGuideActivated;
                    if (!_aimingGuideActivated) {
                        _aimModeOnOff->setText(L(txt::kOptOff));
                        _aimModeOnOff->setColor(0xff0066);
                        _altTextOff->exists = true;
                        _altTextOn->exists = false;
                    } else {
                        _aimModeOnOff->setText(L(txt::kOptOn));
                        _aimModeOnOff->setColor(0x33ff66);
                        _altTextOff->exists = false;
                        _altTextOn->exists = true;
                    }
                } else if (touchOver(_howToPlay)) {
                    FlxG::play("explode.wav");
                    _howToPlayBox->exists = true;
                    saveContext();
                    FlxG::pauseMusic();
                    FlxG::setState(makeTutorialState());
                } else if (touchOver(_viewStats)) {
                    FlxG::play("explode.wav");
                    _viewStatsBox->exists = true;
                    saveContext();
                    FlxG::pauseMusic();
                    FlxG::setState(makeStatState());
                } else if (touchOver(_playNow)) {
                    FlxG::play("gameOver.wav");
                    FlxG::pauseMusic();
                    saveContext();
                    _silhouette->exists = true;
                } else if (touchOver(_twitter)) {
                    onTweet();
                    _tweetTime = 0;
                } else if (touchOver(_facebook)) {
                    onFacebook();
                    _tweetTime = 0;
                } else if (touchOver(_OFbutton)) {
                    FlxG::play("shoot.wav");
                    // PORT: [OpenFeint launchDashboard] — service shut down in 2012.
                } else if (touchOver(_GCbutton)) {
                    FlxG::play("enemyShoot.wav");
                    // PORT: [[UIApplication sharedApplication] openURL:@"gamecenter:"] — no Game Center here.
                }
            }
            scrollClouds(_mainCloud, _cloud2, _cloud3, _cloud4);
            _tweetTime = _tweetTime + FlxG::elapsed();
            FlxState::update();
        } else {
            _silhouetteTimer = _silhouetteTimer - FlxG::elapsed();
            if (0.0f < _silhouetteTimer && !FlxG::touches()->touchesBegan) {
                _tweetTime = _tweetTime + FlxG::elapsed();
                FlxState::update();
                return;
            }
            FlxG::setState(playStateFromDefaults(false));
        }
    }

    // -[OptionsState saveContext]
    void saveContext() {
        UserDefaults& d = UserDefaults::standard();
        d.set("preferredControlScheme", _selectedScheme == 0 ? "precision" : "slingshot");
        d.set("aimGuide", _aimingGuideActivated ? "on" : "off");
        d.synchronize();
    }

    // -[OptionsState onTweet] / -onFacebook
    // PORT: ShareKit/Facebook are never authorised any more, so the original took the
    // "[twitter authorize]" / "[delegate facebookLogin]" branch: click sound, then a dead login flow.
    void onTweet() { FlxG::play("shoot.wav"); }
    void onFacebook() { FlxG::play("shoot.wav"); }

    // -[OptionsState getUpdateStrings]: builds the tweet text from Statistics (only used by the dead
    // tweet dialog). PORT: nothing to build.
    void getUpdateStrings() {}
};

}  // namespace

FlxState* makeOptionsState() { return (new OptionsState)->init(); }
