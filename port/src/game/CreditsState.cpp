// CreditsState — scrolling staff credits over the sunset sky.
// Reconstructed from the ZiGGURAT 1.0.1 binary (armv7), see re/decomp_softfp/game/CreditsState/.
#include "game/MenuCommon.h"

using namespace zig;

namespace {

class CreditsState : public FlxState {
public:
    FlxSprite *_sky = nullptr, *_cloud4 = nullptr, *_cloud3 = nullptr, *_cloud2 = nullptr, *_mainCloud = nullptr;
    FlxGroup* _texts = nullptr;
    float _scrollLocation = 0;

    // -[CreditsState addText:]  ea=00060500
    void addText(const std::string& text) {
        FlxText* t = newText((unsigned int)FlxG::width(), text, 24.0f);
        t->setY(-100.0f);
        t->setAlignment("center");
        _texts->add(t);
    }

    // -[CreditsState create]  ea=0005f848
    void create() override {
        FlxG::playMusic("Overture.mp3");
        _scrollLocation = (float)(-FlxG::height());
        _sky = newSprite("sky_sunset.png");
        _sky->setX(halfW() + _sky->getWidth() * -0.5f);
        _sky->setY((float)FlxG::height() - _sky->getHeight());
        add(_sky);
        _cloud4 = newSprite("clouds4_sunset.png");
        _cloud4->setX(0);
        _cloud4->setY((float)(FlxG::height() - 128));
        add(_cloud4);
        _cloud3 = newSprite("clouds3_sunset.png");
        _cloud3->setX(0);
        _cloud3->setY((float)(FlxG::height() - 115));
        add(_cloud3);
        _cloud2 = newSprite("clouds2_sunset.png");
        _cloud2->setX(0);
        _cloud2->setY((float)(FlxG::height() - 84));
        add(_cloud2);

        _texts = (new FlxGroup)->init();
        addText(L(txt::kCreditsTitle));
        addText("");
        addText(L(txt::kCreditsByline));
        addText("");
        addText("");
        addText(L(txt::kCreditsStaff));
        addText("");
        addText(L(txt::kCreditsDirector));
        addText("tim rogers");
        addText("");
        addText(L(txt::kCreditsProgram));
        addText("michael kerwin");
        addText("");
        addText(L(txt::kCreditsGraphics));
        addText("brent porter");
        addText("");
        addText(L(txt::kCreditsSound));
        addText("andrew toups");
        addText("");
        addText(L(txt::kCreditsThanks));
        addText("bennett foddy");
        addText("tracey lien");
        addText("zak mccune");
        addText("");
        addText(L(txt::kCreditsWebsite));
        addText("zggrt.com");
        add(_texts);

        _mainCloud = newSprite("mainClouds.png");
        _mainCloud->setX(0);
        _mainCloud->setY((float)(FlxG::height() - 38));
        add(_mainCloud);
    }

    // -[CreditsState update]  ea=0005ffc0
    void update() override {
        if (FlxG::touches()->touchesBegan ||
            (float)((int)_texts->members.size() * 32 + 50) < _scrollLocation) {
            FlxG::setState(makeOptionsState());
        }
        _scrollLocation = _scrollLocation + FlxG::elapsed() * 30.0f;
        int yy = 0;
        for (FlxObject* t : _texts->members) {
            const float fy = (float)yy;
            yy += 32;
            t->setY(fy - _scrollLocation);
        }
        scrollClouds(_mainCloud, _cloud2, _cloud3, _cloud4);
        FlxState::update();
    }

    // -[CreditsState onBack] (unreferenced: update returns to Options by itself)
    void onBack() {
        FlxG::playMusic("Overture.mp3");
        FlxG::setState(makeOptionsState());
    }
};

}  // namespace

FlxState* makeCreditsState() { return (new CreditsState)->init(); }
