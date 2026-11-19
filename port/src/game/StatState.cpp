// StatState — scrolling lifetime statistics, tweet/facebook best score, credits, back.
// Reconstructed from the ZiGGURAT 1.0.1 binary (armv7), see re/decomp_softfp/game/StatState/.
#include <cstdio>

#include "game/MenuCommon.h"

using namespace zig;

namespace {

class StatState : public FlxState {
public:
    float _scrollLocation = 0, _scrollSpeed = 0;
    int _touchY = 0, _lastTouchY = 0;
    int _bestKillsInt = 0, _bestShotsInt = 0, _bestSecondsInt = 0;
    bool _touched = false;
    FlxSprite* _backgroundImage = nullptr;
    FlxButton* _backButton = nullptr;
    FlxText *_tweetButton = nullptr, *_facebookButton = nullptr, *_creditsButton = nullptr;
    FlxGroup *_descriptions = nullptr, *_numerals = nullptr;
    float _statStateTime = 0;

    // description / numeral pairs (ivar names of the original)
    FlxText *_averageKills, *_averageKillsNum, *_averageSeconds, *_averageSecondsNum, *_averageShots, *_averageShotsNum;
    FlxText *_bestKills, *_bestKillsNum, *_bestSeconds, *_bestSecondsNum, *_bestShots, *_bestShotsNum;
    FlxText *_bestAccuracy, *_bestAccuracyNum, *_bestUfoHits, *_bestUfoHitsNum;
    FlxText *_blueDeaths, *_blueDeathsNum, *_blueKills, *_blueKillsNum, *_bulletDeaths, *_bulletDeathsNum;
    FlxText *_gamesPlayed, *_gamesPlayedNum, *_giantDeaths, *_giantDeathsNum, *_giantKills, *_giantKillsNum;
    FlxText *_headShots, *_headShotsNum, *_kills, *_killsNum;
    FlxText *_purpleDeaths, *_purpleDeathsNum, *_purpleKills, *_purpleKillsNum;
    FlxText *_bigPurpleDeaths, *_bigPurpleDeathsNum, *_bigPurpleKills, *_bigPurpleKillsNum;
    FlxText *_redDeaths, *_redDeathsNum, *_orangeDeaths, *_orangeDeathsNum, *_redKills, *_redKillsNum;
    FlxText *_orangeKills, *_orangeKillsNum, *_secondsPlayed, *_secondsPlayedNum, *_shots, *_shotsNum;
    FlxText *_accuracy, *_accuracyNum, *_ufoHits, *_ufoHitsNum, *_ufoKills, *_ufoKillsNum;
    FlxText *_yellowDeaths, *_yellowDeathsNum, *_yellowKills, *_yellowKillsNum, *_moonExplosions, *_moonExplosionsNum;

    FlxText* desc(txt::Id key, float size = 14.0f) { return newText((unsigned int)FlxG::width(), L(key), size); }
    FlxText* num(const std::string& text = std::string()) { return newText((unsigned int)(FlxG::width() - 10), text, 14.0f); }
    void row(FlxText* d, FlxText* n) { _descriptions->add(d); _numerals->add(n); }
    void blankRow() { _descriptions->add(num("")); _numerals->add(num("")); }

    // -[StatState create]  ea=00050224
    void create() override {
        FlxG::playMusic("Stelegy.mp3");
        _statStateTime = 0;
        _scrollLocation = (float)(-FlxG::height());
        _scrollSpeed = 30.0f;
        _touchY = 0;
        _lastTouchY = 0;
        _touched = false;
        _backgroundImage = newSprite("sky_night.png");
        _backgroundImage->setX(halfW() + _backgroundImage->getWidth() * -0.5f);
        _backgroundImage->setY((float)FlxG::height() - _backgroundImage->getHeight());
        _descriptions = (new FlxGroup)->init();
        _numerals = (new FlxGroup)->init();

        _averageKills = desc(txt::kStatAvgKills);                 _averageKillsNum = num();
        _averageSeconds = desc(txt::kStatAvgSeconds);   _averageSecondsNum = num();
        _averageShots = desc(txt::kStatAvgShots);          _averageShotsNum = num();
        _bestKills = desc(txt::kStatBestKills);                                  _bestKillsNum = num();
        _bestSeconds = desc(txt::kStatBestSeconds);                        _bestSecondsNum = num();
        _bestShots = desc(txt::kStatBestShots);                            _bestShotsNum = num();
        _bestAccuracy = desc(txt::kStatBestAccuracy);                            _bestAccuracyNum = num();
        float ufoSize;
        if (lang("de") || lang("pt-BR")) ufoSize = 11.0f;
        else if (lang("it") || lang("pt") || lang("pt-PT")) ufoSize = 9.0f;
        else ufoSize = 14.0f;
        _bestUfoHits = desc(txt::kStatBestUfoHits, ufoSize);  _bestUfoHitsNum = num();
        _blueDeaths = desc(txt::kStatBlueDeaths);                       _blueDeathsNum = num();
        _blueKills = desc(txt::kStatBlueKills);                 _blueKillsNum = num();
        _bulletDeaths = desc(txt::kStatBulletDeaths);                  _bulletDeathsNum = num();
        _gamesPlayed = desc(txt::kStatGames);                   _gamesPlayedNum = num();
        _giantDeaths = desc(txt::kStatGiantDeaths);                       _giantDeathsNum = num();
        _giantKills = desc(txt::kStatGiantKills);                 _giantKillsNum = num();
        _headShots = desc(txt::kStatHeadshots);                        _headShotsNum = num();
        _kills = desc(txt::kStatKills);                                _killsNum = num();
        _purpleDeaths = desc(txt::kStatPurpleDeaths);                   _purpleDeathsNum = num();
        _bigPurpleDeaths = desc(txt::kStatBigPurpleDeaths);            _bigPurpleDeathsNum = num();
        _purpleKills = desc(txt::kStatPurpleKills);             _purpleKillsNum = num();
        _bigPurpleKills = desc(txt::kStatBigPurpleKills);      _bigPurpleKillsNum = num();
        _redDeaths = desc(txt::kStatRedDeaths);                         _redDeathsNum = num();
        _orangeDeaths = desc(txt::kStatOrangeDeaths);                   _orangeDeathsNum = num();
        _redKills = desc(txt::kStatRedKills);                   _redKillsNum = num();
        _orangeKills = desc(txt::kStatOrangeKills);             _orangeKillsNum = num();
        _secondsPlayed = desc(txt::kStatSeconds);             _secondsPlayedNum = num();
        _shots = desc(txt::kStatShots);                          _shotsNum = num();
        _accuracy = desc(txt::kStatAccuracy);                          _accuracyNum = num();
        _ufoHits = desc(txt::kStatUfoHits);       _ufoHitsNum = num();
        _ufoKills = desc(txt::kStatUfoKills);             _ufoKillsNum = num("0");
        _yellowDeaths = desc(txt::kStatYellowDeaths);                   _yellowDeathsNum = num();
        _yellowKills = desc(txt::kStatYellowKills);             _yellowKillsNum = num();
        _moonExplosions = desc(txt::kStatMoon);  _moonExplosionsNum = num();
        FlxText* total = desc(txt::kStatTotal);
        FlxText* average = desc(txt::kStatAverage);
        FlxText* best = desc(txt::kStatBest);
        FlxText* deathsBy = desc(txt::kStatDeathsBy);

        row(total, num(""));
        row(_kills, _killsNum);
        row(_gamesPlayed, _gamesPlayedNum);
        row(_secondsPlayed, _secondsPlayedNum);
        row(_shots, _shotsNum);
        row(_accuracy, _accuracyNum);
        row(_headShots, _headShotsNum);
        row(_moonExplosions, _moonExplosionsNum);
        blankRow();
        row(_blueKills, _blueKillsNum);
        row(_yellowKills, _yellowKillsNum);
        row(_redKills, _redKillsNum);
        row(_orangeKills, _orangeKillsNum);
        row(_giantKills, _giantKillsNum);
        row(_purpleKills, _purpleKillsNum);
        row(_bigPurpleKills, _bigPurpleKillsNum);
        blankRow();
        row(average, num(""));
        row(_averageKills, _averageKillsNum);
        row(_averageSeconds, _averageSecondsNum);
        row(_averageShots, _averageShotsNum);
        blankRow();
        row(best, num(""));
        row(_bestKills, _bestKillsNum);
        row(_bestSeconds, _bestSecondsNum);
        row(_bestShots, _bestShotsNum);
        row(_bestAccuracy, _bestAccuracyNum);
        blankRow();
        blankRow();
        blankRow();
        blankRow();
        row(_ufoHits, _ufoHitsNum);
        row(_bestUfoHits, _bestUfoHitsNum);
        row(_ufoKills, _ufoKillsNum);
        blankRow();
        row(deathsBy, num(""));
        row(_bulletDeaths, _bulletDeathsNum);
        row(_blueDeaths, _blueDeathsNum);
        row(_yellowDeaths, _yellowDeathsNum);
        row(_redDeaths, _redDeathsNum);
        row(_orangeDeaths, _orangeDeathsNum);
        row(_giantDeaths, _giantDeathsNum);
        row(_purpleDeaths, _purpleDeathsNum);
        row(_bigPurpleDeaths, _bigPurpleDeathsNum);

        int yy = 0;
        for (size_t i = 0; i < _descriptions->members.size(); i++) {
            auto* d = static_cast<FlxText*>(_descriptions->members[i]);
            d->setColor(0xffffffff);
            d->setAlignment("left");
            d->setX(10.0f);
            const float fy = (float)yy;
            d->setY(fy - _scrollLocation);
            auto* n = static_cast<FlxText*>(_numerals->members[i]);
            n->setColor(0xffffffff);
            n->setAlignment("right");
            n->setX(0);
            n->setY(fy - _scrollLocation);
            yy += 24;
        }
        updateStrings();

        float shareSize;
        if (lang("fr") || lang("de") || lang("es") || lang("pt-BR")) shareSize = 16.0f;
        else if (lang("it") || lang("pt") || lang("pt-PT")) shareSize = 14.0f;
        else shareSize = 20.0f;
        _tweetButton = newText(300, L(txt::kStatTweet), shareSize);
        _tweetButton->setAlignment("center");
        _tweetButton->setColor(0xffccff00);
        _tweetButton->setX(halfW() - _tweetButton->getWidth() * 0.5f);
        add(_tweetButton);
        _facebookButton = newText(300, L(txt::kStatFacebook), shareSize);
        _facebookButton->setAlignment("center");
        _facebookButton->setColor(0xffccff00);
        _facebookButton->setX(halfW() - _facebookButton->getWidth() * 0.5f);
        add(_facebookButton);
        const float creditsSize = (lang("fr") || lang("de")) ? 16.0f : 20.0f;
        _creditsButton = newText(290, L(txt::kStatCredits), creditsSize);
        _creditsButton->setAlignment("center");
        _creditsButton->setColor(0xffccff00);
        _creditsButton->setX(halfW() - _creditsButton->getWidth() * 0.5f);
        add(_creditsButton);
        // PORT: tweet/facebook confirm dialog omitted (unreachable without a logged-in ShareKit/Facebook).
        _backButton = newImageButton(FlxG::width() - 136, 10, FlashFunction::functionWith([this] { onBack(); }), L(txt::kBack), 18.0f);
        add(_backButton);
        _backButton->exists = false;
    }

    // -[StatState updateStrings]  ea=000546b0
    void updateStrings() {
        Statistics& s = Statistics::shared();
        char buf[256];
        auto fmt = [&](txt::Id key, int v) {
            snprintf(buf, sizeof buf, L(key).c_str(), v);
            return std::string(buf);
        };
        auto i = [&](int v) { snprintf(buf, sizeof buf, "%i", v); return std::string(buf); };
        double accuracy = 0, bestAccuracy = 0;  // DAT_00054794
        if (s.getFloat("shots") != 0.0f)
            accuracy = (double)((1.0f - s.getFloat("missedShots") / s.getFloat("shots")) * 100.0f);
        if (s.getFloat("bestShots") != 0.0f)
            bestAccuracy = (double)((1.0f - s.getFloat("bestMissedShots") / s.getFloat("bestShots")) * 100.0f);
        _gamesPlayedNum->setText(fmt(txt::kFmtGames, s.getInt("gamesPlayed")));
        _killsNum->setText(fmt(txt::kFmtKills, s.getInt("kills")));
        _secondsPlayedNum->setText(fmt(txt::kFmtSeconds, s.getInt("secondsPlayed")));
        _shotsNum->setText(fmt(txt::kFmtShots, s.getInt("shots")));
        snprintf(buf, sizeof buf, "%1.1f%s", accuracy, "%");
        _accuracyNum->setText(buf);
        _averageKillsNum->setText(fmt(txt::kFmtKills, s.getInt("averageKills")));
        _averageSecondsNum->setText(fmt(txt::kFmtSeconds, s.getInt("averageSeconds")));
        _averageShotsNum->setText(fmt(txt::kFmtShots, s.getInt("averageShots")));
        _bestKillsNum->setText(fmt(txt::kFmtKills, s.getInt("bestKills")));
        _bestSecondsNum->setText(fmt(txt::kFmtSeconds, s.getInt("bestSeconds")));
        _bestShotsNum->setText(fmt(txt::kFmtShots, s.getInt("bestShots")));
        snprintf(buf, sizeof buf, "%1.1f%s", bestAccuracy, "%");
        _bestAccuracyNum->setText(buf);
        _bulletDeathsNum->setText(i(s.getInt("bulletDeaths")));
        _blueDeathsNum->setText(i(s.getInt("blueDeaths")));
        _yellowDeathsNum->setText(i(s.getInt("yellowDeaths")));
        _redDeathsNum->setText(i(s.getInt("redDeaths")));
        _orangeDeathsNum->setText(i(s.getInt("orangeDeaths")));
        _giantDeathsNum->setText(i(s.getInt("giantDeaths")));
        _blueKillsNum->setText(i(s.getInt("blueKills")));
        _yellowKillsNum->setText(i(s.getInt("yellowKills")));
        _redKillsNum->setText(i(s.getInt("redKills")));
        _orangeKillsNum->setText(i(s.getInt("orangeKills")));
        _giantKillsNum->setText(i(s.getInt("giantKills")));
        _purpleKillsNum->setText(i(s.getInt("purpleKills")));
        _purpleDeathsNum->setText(i(s.getInt("purpleDeaths")));
        _bigPurpleKillsNum->setText(i(s.getInt("bigPurpleKills")));
        _bigPurpleDeathsNum->setText(i(s.getInt("bigPurpleDeaths")));
        _ufoHitsNum->setText(i(s.getInt("ufoHits")));
        _bestUfoHitsNum->setText(i(s.getInt("bestUfoHits")));
        _headShotsNum->setText(fmt(txt::kFmtHeadshots, s.getInt("headShots")));
        _moonExplosionsNum->setText(i(s.getInt("moonExplosions")));
        // hide the labels of enemies the player has never met
        if (s.getInt("purpleDeaths") == 0 && s.getInt("purpleKills") == 0) {
            _purpleKills->setText(". . . ?????????");
            _purpleDeaths->setText(". . . ?????????");
        }
        if (s.getInt("bigPurpleDeaths") == 0 && s.getInt("bigPurpleKills") == 0) {
            _bigPurpleKills->setText(". . . ?????????");
            _bigPurpleDeaths->setText(". . . ?????????");
        }
        if (s.getInt("moonExplosions") == 0) _moonExplosions->setText(". . . ?????????");
        _bestKillsInt = s.getInt("bestKills");
        _bestShotsInt = s.getInt("bestShots");
        _bestSecondsInt = s.getInt("bestSeconds");
        // (tweetString built here in the original — only used by the dead tweet dialog)
    }

    // -[StatState update]  ea=00055988
    void update() override {
        _statStateTime = _statStateTime + FlxG::elapsed();
        if (0.3 < (double)_statStateTime) _backButton->exists = true;  // DAT_00055ca8 (double), && !twCancel.exists
        FlxTouches* t = FlxG::touches();
        if (t->touchesBegan) {
            _touchY = (int)t->touchPoint().y;
            _lastTouchY = (int)t->touchPoint().y;
            _touched = true;
        } else if (t->touching) {
            _lastTouchY = _touchY;
            _touchY = (int)t->touchPoint().y;
            _scrollSpeed = (float)((_lastTouchY - _touchY) * 40);
        }
        _scrollLocation = _scrollLocation + _scrollSpeed * FlxG::elapsed();
        if (_touched) _scrollSpeed = (float)((double)_scrollSpeed / 1.1);  // DAT_00055fec (double)
        const float minLoc = (float)(24 - FlxG::height());
        if (minLoc <= _scrollLocation) {
            const float maxLoc = (float)((int)_descriptions->members.size() * 24 + 60 - FlxG::height());
            if (maxLoc < _scrollLocation) _scrollLocation = maxLoc;
        } else {
            _scrollLocation = minLoc;
        }
        int yy = 0;
        for (size_t i = 0; i < _descriptions->members.size(); i++) {
            const float fy = (float)yy;
            yy += 24;
            _descriptions->members[i]->setY(fy - _scrollLocation);
            _numerals->members[i]->setY(fy - _scrollLocation);
        }
        if (t->touchesBegan) {
            if (touchOver(_tweetButton)) onTweet();
            else if (touchOver(_facebookButton)) onFacebook();
            else if (touchOver(_creditsButton)) {
                FlxG::pauseMusic();
                FlxG::setState(makeCreditsState());
            }
        }
        _descriptions->update();
        _numerals->update();
        FlxState::update();
    }

    // -[StatState render]  ea=00056174
    void render() override {
        _tweetButton->setY(_bestAccuracy->y + 48.0f);
        _facebookButton->setY(_tweetButton->y + 36.0f);  // DAT_000562cc
        _creditsButton->setY(_bigPurpleDeaths->y + 48.0f);
        _backgroundImage->render();
        _descriptions->render();  // (skipped while the tweet dialog is open)
        _numerals->render();
        FlxState::render();
    }

    // -[StatState onBack]
    void onBack() {
        FlxG::playMusic("Overture.mp3");
        FlxG::setState(makeOptionsState());
    }

    // PORT: dead social features — click sound only (ShareKit/Facebook never authorised).
    void onTweet() { FlxG::play("shoot.wav"); }
    void onFacebook() { FlxG::play("shoot.wav"); }
};

}  // namespace

FlxState* makeStatState() { return (new StatState)->init(); }
