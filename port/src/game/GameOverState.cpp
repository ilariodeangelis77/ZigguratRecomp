// GameOverState — final score sentence + tweet / facebook / retry buttons.
// Reconstructed from the ZiGGURAT 1.0.1 binary (armv7), see re/decomp_softfp/game/GameOverState/.
#include <cstdio>

#include "game/Enhancements.h"
#include "game/MenuCommon.h"

using namespace zig;

namespace {

class GameOverState : public FlxState {
public:
    int _screenState = 0;
    float _stateTime = 0;
    unsigned int _score = 0;
    FlxText* _scoreText = nullptr;
    FlxSprite *_OFbutton = nullptr, *_GCbutton = nullptr, *_optionsButton = nullptr, *_silhouette = nullptr;
    const plat::Texture* _deathScreen = nullptr;  // stored but never drawn by the original either
    FlxButton *_tweetButton = nullptr, *_facebookButton = nullptr, *_retryButton = nullptr;
    std::string _tweetString;

    // -[GameOverState initWithScore:withImage:withManagedObjectContext:]  ea=00027074
    GameOverState* initWithScore(unsigned int score, const plat::Texture* image) {
        FlxState::init();
        setBgColor(0xff000000);
        _score = score;
        _deathScreen = image;
        return this;
    }

    // -[GameOverState numToWords:]  ea=00029d70 — English words for en, "%i" otherwise.
    std::string numToWords(int n) {
        if (preferredLanguage() != "en") return std::to_string(n);
        if (n == 0) return "none";
        int thousands = 0, hundreds = 0, tens = 0;
        for (; 999 < n; n -= 1000) thousands++;
        for (; 99 < n; n -= 100) hundreds++;
        for (; 9 < n; n -= 10) tens++;
        static const char* const kThousands[] = {"", "one thousand", "two thousand", "three thousand", "four thousand",
                                                 "five thousand", "six thousand", "seven thousand", "eight thousand",
                                                 "nine thousand"};
        static const char* const kHundreds[] = {"", "one hundred", "two hundred", "three hundred", "four hundred",
                                                "five hundred", "six hundred", "seven hundred", "eight hundred",
                                                "nine hundred"};
        static const char* const kTens[] = {"", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"};
        static const char* const kTeens[] = {"ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen",
                                             "seventeen", "eighteen", "nineteen"};
        static const char* const kUnits[] = {"", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine"};
        std::string s;
        if (thousands >= 1 && thousands <= 9) {
            s += kThousands[thousands];
            if (hundreds > 0 || tens > 0 || n > 0) s += " ";
        }
        if (hundreds >= 1 && hundreds <= 9) {
            s += kHundreds[hundreds];
            if (tens > 0 || n > 0) s += " ";
        }
        if (tens == 1) return s + kTeens[n];
        if (tens >= 2 && tens <= 9) {
            s += kTens[tens];
            if (n > 0) s += "-";
        }
        if (n >= 1 && n <= 9) s += kUnits[n];
        return s;
    }

    // -[GameOverState create]  ea=000270f8
    void create() override {
        _tweetString = L(txt::kOverShareStart) + numToWords((int)_score) +
                       L(txt::kOverShareEnd);
        _screenState = 0;
        _stateTime = 0;
        const std::string scoreLine = L(txt::kOverScoreStart) + numToWords((int)_score) +
                                      L(txt::kOverScoreEnd);
        _scoreText = (new FlxText)->initWithX(0.0f, 100.0f, (unsigned int)FlxG::width(), scoreLine);
        _scoreText->setSize(16.0f);
        _scoreText->setAlignment("center");
        _scoreText->visible = false;
        add(_scoreText);

        const int gap = (FlxG::width() - 378) / 4;  // three 126-px buttons spread across the screen
        _tweetButton = (new FlxButton)->initWithX(gap, 200, FlashFunction::functionWith([this] { onTweet(); }));
        _tweetButton->loadGraphicWithParam1(FlxSprite::spriteWithGraphic("img_button.png"));
        FlxText* tweetLabel = newText((unsigned int)_tweetButton->getWidth(), L(txt::kOverTweet), 16.0f);
        tweetLabel->setAlignment("center");
        _tweetButton->loadTextWithParam1(tweetLabel);
        _tweetButton->exists = false;
        add(_tweetButton);

        const float fbSize = (lang("pt") || lang("pt-PT")) ? 13.0f : 16.0f;
        _facebookButton = newImageButton(gap * 2 + 126, 200, FlashFunction::functionWith([this] { onFacebook(); }),
                                         L(txt::kOverFacebook), fbSize);
        _facebookButton->exists = false;
        add(_facebookButton);

        const float retrySize = lang("fr") ? 13.0f : 16.0f;  // DAT_000276b4 / DAT_000276b8
        _retryButton = newImageButton(gap * 3 + 252, 200, FlashFunction::functionWith([this] { onRetry(); }),
                                      L(txt::kOverRetry), retrySize);
        _retryButton->exists = false;
        add(_retryButton);
        // PORT: tweet/facebook confirm dialog (confirm1-3 texts, cancel/logout/confirm buttons) omitted:
        // it only opened for an authorised ShareKit/Facebook session, which can no longer exist.

        _optionsButton = FlxSprite::spriteWithGraphic("img_options.png");
        _optionsButton->setX((float)FlxG::width() - _optionsButton->getWidth() + -10.0f);
        _optionsButton->setY((float)FlxG::height() + _optionsButton->getHeight());
        add(_optionsButton);
        _OFbutton = FlxSprite::spriteWithGraphic("img_OFlogo.png");
        _OFbutton->setX(10.0f);
        _OFbutton->setY((float)FlxG::height() + _optionsButton->getHeight() + 15.0f);
        add(_OFbutton);
        _GCbutton = FlxSprite::spriteWithGraphic("img_GClogo.png");
        _GCbutton->setX(55.0f);
        _GCbutton->setY((float)FlxG::height() + _optionsButton->getHeight() + 15.0f);
        add(_GCbutton);

        _silhouette = newSprite("img_silhouette.png");
        _silhouette->setX(halfW() - _silhouette->getWidth() * 0.5f);
        _silhouette->setY((float)FlxG::height() - _silhouette->getHeight());
        _silhouette->exists = false;
        add(_silhouette);
    }

    // -[GameOverState update]  ea=0002895c
    // ENHANCEMENT showDeathSnapshot: draw the frame PlayState -snapshot: captured (the original stores it
    // but never draws it) as a dimmed backdrop behind the game-over text.
    void render() override {
        if (enh().showDeathSnapshot && _deathScreen && _deathScreen->tex)
        {
            plat::draw(_deathScreen, 0, 0, (float)_deathScreen->w, (float)_deathScreen->h, 0, 0, (float)FlxG::width(),
                       (float)FlxG::height());
            plat::fillRect(0, 0, (float)FlxG::width(), (float)FlxG::height(), 0xA6000000);  // dim to ~35 %
        }
        FlxState::render();
    }

    void update() override {
        if (_silhouette->exists) {
            FlxG::setState(playStateFromDefaults(false));
            return;
        }
        if (_screenState == 2) {
            if ((float)FlxG::height() - _optionsButton->getHeight() < _optionsButton->y && _scoreText->visible) {
                const float k = 120.0f;  // DAT_00028f6c
                _optionsButton->setY(_optionsButton->y - FlxG::elapsed() * k);
                _OFbutton->setY(_OFbutton->y - FlxG::elapsed() * k);
                _GCbutton->setY(_GCbutton->y - FlxG::elapsed() * k);
            }
            // NB: the original tests the current touchPoint without checking touchesBegan.
            if (touchOver(_optionsButton)) {
                FlxG::playMusic("Overture.mp3");
                FlxG::play("shoot.wav");
                FlxG::setState(makeOptionsState());
            } else if (FlxG::touches()->touchesBegan && touchOver(_OFbutton)) {
                // PORT: [OpenFeint launchDashboard] is gone; also gated on touchesBegan so the stale
                // touchPoint does not retrigger the click every frame (the dashboard used to cover it).
                FlxG::play("shoot.wav");
            } else if (FlxG::touches()->touchesBegan && touchOver(_GCbutton)) {
                FlxG::play("enemyShoot.wav");  // PORT: openURL:@"gamecenter:" dropped
            }
        } else if (_screenState == 1) {
            if (1.5f <= _stateTime) {
                _tweetButton->exists = true;
                _facebookButton->exists = true;
                _retryButton->exists = true;
                _screenState = 2;
                _stateTime = 0;
            }
        } else if (_screenState == 0) {
            if (_stateTime == 0.0f) {
                FlxG::play("gameOver.wav");
            } else if (1.5f <= _stateTime) {
                _scoreText->visible = true;
                _screenState = 1;
                _stateTime = 0;
                FlxG::play("gameOver.wav");
            }
        }
        _stateTime = _stateTime + FlxG::elapsed();
        FlxState::update();
    }

    // -[GameOverState onRetry]  ea=000296c0
    void onRetry() {
        _silhouette->exists = true;
        FlxG::play("gameOver.wav");
    }

    // PORT: dead social features — click sound only (never authorised).
    void onTweet() { FlxG::play("shoot.wav"); }
    void onFacebook() { FlxG::play("shoot.wav"); }
};

}  // namespace

FlxState* makeGameOverState(unsigned int score, const plat::Texture* deathScreen) {
    return (new GameOverState)->initWithScore(score, deathScreen);
}
