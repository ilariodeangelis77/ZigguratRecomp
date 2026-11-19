// Small helpers shared by the ZiGGURAT menu/meta states (StartState, TutorialState, OptionsState,
// StatState, CreditsState, GameOverState). They only wrap idioms that repeat verbatim in the
// decompiled ObjC; every call keeps the original order of operations.
#pragma once
#include <string>

#include "game/Shared.h"

namespace zig {

// [[FlxSprite alloc] init] + loadGraphic:
inline FlxSprite* newSprite(const std::string& graphic) {
    FlxSprite* s = (new FlxSprite)->init();
    s->loadGraphic(graphic);
    return s;
}

// [FlxText textWithWidth:Width text:Text font:nil size:Size]
inline FlxText* newText(unsigned int width, const std::string& text, float size) {
    return FlxText::textWithWidth(width, text, std::string(), size);
}

// FlxG.width / 2 as ObjC computes it: integer division, then int->float.
inline float halfW() { return (float)(FlxG::width() / 2); }
inline float halfH() { return (float)(FlxG::height() / 2); }

// [[[NSUserDefaults standardUserDefaults] stringForKey:key] compare:value] == 0
// NB: messaging nil returns 0, so a missing key compares "equal" (original behaviour).
inline bool prefEqualsOrNil(const char* key, const char* value) {
    UserDefaults& d = UserDefaults::standard();
    return !d.has(key) || d.get(key) == value;
}

// The block repeated in StartState/OptionsState/GameOverState:
// [[PlayState alloc] initWithScheme:(scheme == "slingshot") withAimGuide:(aimGuide == "on")
//                       withGodMode:godMode withManagedObjectContext:moc]
inline FlxState* playStateFromDefaults(bool godMode) {
    bool slingshot = prefEqualsOrNil("preferredControlScheme", "slingshot");
    bool aimGuide = prefEqualsOrNil("aimGuide", "on");
    return makePlayState(slingshot ? 1 : 0, aimGuide, godMode);
}

// [obj overlapsPointWithParam1:FlxG.touches.touchPoint.x param2:FlxG.touches.touchPoint.y]
inline bool touchOver(FlxObject* o) {
    CGPoint p = FlxG::touches()->touchPoint();
    return o->overlapsPointWithParam1(p.x, p.y);
}

inline bool lang(const char* code) { return preferredLanguage() == code; }

// Parallax cloud scroll used verbatim by StartState/OptionsState/CreditsState -update:
//   x += elapsed * speed; if (x >= 0) x += wrap   (mainCloud wraps by -width/2)
inline void scrollClouds(FlxSprite* mainCloud, FlxSprite* cloud2, FlxSprite* cloud3, FlxSprite* cloud4) {
    float e;
    e = FlxG::elapsed() * 60.0f;
    mainCloud->setX(mainCloud->x + e);
    if (mainCloud->x >= 0) mainCloud->setX(mainCloud->x + mainCloud->getWidth() * -0.5f);
    e = FlxG::elapsed() * 40.0f;
    cloud2->setX(cloud2->x + e);
    if (cloud2->x >= 0) cloud2->setX(cloud2->x + -75.0f);
    e = FlxG::elapsed() * 20.0f;
    cloud3->setX(cloud3->x + e);
    if (cloud3->x >= 0) cloud3->setX(cloud3->x + -59.0f);
    e = FlxG::elapsed() * 10.0f;
    cloud4->setX(cloud4->x + e);
    if (cloud4->x >= 0) cloud4->setX(cloud4->x + -62.0f);
}

// FlxButton with img_button.png and a centred label, as built in Stat/Options/GameOver -create.
inline FlxButton* newImageButton(int x, int y, FlashFunction* cb, const std::string& label, float size) {
    FlxButton* b = (new FlxButton)->initWithX(x, y, cb);
    b->loadGraphicWithParam1(FlxSprite::spriteWithGraphic("img_button.png"));
    FlxText* t = newText((unsigned int)b->getWidth(), label, size);
    b->loadTextWithParam1(t);
    return b;
}

}  // namespace zig
