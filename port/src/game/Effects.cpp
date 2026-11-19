// GibCluster, SmallExpEmitter, MoonRock, UFO, BGManager.
// Reconstructed from ZiGGURAT (iOS) — see Gameplay.h for conventions.
#include <cmath>

#include "game/Enhancements.h"
#include "game/Gameplay.h"

// ================================================================ GibCluster
// - (id) initWithColor:(NSString *)color   @0x4c370
GibCluster* GibCluster::initWithColor(const std::string& color) {
    FlxGroup::init();
    std::string suffix = color + ".png";
    auto mk = [&](const char* prefix) {
        auto* s = (new FlxSprite)->init();
        s->loadGraphic(std::string(prefix) + suffix);
        return s;
    };
    _head = mk("img_head_");
    _Larm = mk("img_Larm_");
    _Rarm = mk("img_Rarm_");
    _torso = mk("img_torso_");
    _Lleg = mk("img_Lleg_");
    _Rleg = mk("img_Rleg_");
    add(_head);
    add(_Larm);
    add(_Rarm);
    add(_torso);
    add(_Lleg);
    add(_Rleg);
    for (auto* m : members) m->exists = false;
    return this;
}

// - (void) resetWithX:(float)x y:(float)y   @0x4c72c
void GibCluster::resetWithXY(float X, float Y) {
    exists = true;
    _timer = 2.0f;
    for (auto* m : members) {
        m->exists = true;
        m->setX(X);
        m->setY(Y);
        m->acceleration = CGPointMake(0, 400.0f);
        uint32_t r1 = zrand();
        uint32_t r2 = zrand();
        m->velocity = CGPointMake((float)((int)(r1 % 0xc9) - 100), (float)((int)(r2 % 0x1f5) - 400));
    }
    for (int i = 0; i < 6; i++) _gibSpinRate[i] = (int)(zrand() % 0x709) - 900;
}

// - (void) update   @0x4c8fc
void GibCluster::update() {
    if (exists) {
        _timer = _timer - FlxG::elapsed();
        if (0.0f < _timer) {
            if (_timer <= 0.5f) {
                for (auto* m : members)
                    if (!m->flickering()) m->flicker(0.55f);
            }
        } else {
            exists = false;
        }
        for (size_t i = 0; i < members.size() && i < 6; i++) {
            FlxObject* m = members[i];
            if (m->onFloor) {
                m->velocity = CGPointMake(m->velocity.x, -150.0f);
                _gibSpinRate[i] = (int)(zrand() % 0x709) - 900;
            }
            m->angle = m->angle + (float)_gibSpinRate[i] * FlxG::elapsed();
        }
    }
    FlxGroup::update();
}

// ================================================================ SmallExpEmitter
// - (id) initWithLifetime:(float)lifetime   @0x5709c   (-init uses -999 = forever)
SmallExpEmitter* SmallExpEmitter::initWithLifetime(float lifetime) {
    _lifetime = lifetime;
    _emitting = true;
    FlxGroup::init();
    for (int i = 0; i < 7; i++) {
        auto* s = (new FlxSprite)->init();
        s->loadGraphic("img_littleExplode.png", true, false, 0x13, 0x13);
        s->addAnimation("explode", {0, 1}, 11.0f, false);
        s->exists = false;
        add(s);
    }
    return this;
}

int SmallExpEmitter::randRangeFrom(int a, int b) { return (int)(zrand() % (uint32_t)((1 - a) + b)) + a; }

// @0x571c8
void SmallExpEmitter::update() {
    if (_emitting && 0.0f <= _lifetime) {
        _lifetime = _lifetime - FlxG::elapsed();
        if (_lifetime <= 0.0f) _emitting = false;
    }
    for (auto* m : members)
        if (((FlxSprite*)m)->finished) m->exists = false;
    if (_emitting) {
        for (auto* m : members) {
            auto* s = (FlxSprite*)m;
            if (!s->exists) {
                s->exists = true;
                s->setX(x - s->frameWidth * 0.5f + (float)randRangeFrom(-22, 22));
                s->setY(y - s->frameHeight * 0.5f + (float)randRangeFrom(-22, 22));
                s->play("explode", true);
            }
        }
    }
    FlxGroup::update();
}

// ================================================================ MoonRock
// - init   @0x56b10
MoonRock* MoonRock::initMoonRock() {
    FlxSprite::initWithX(60.0f, 0.0f, std::string());
    loadGraphic("img_moonRock.png");
    _bounces = 1;
    acceleration = CGPointMake(0, 240.0f);
    collideBottom = false;
    exists = false;
    return this;
}
int MoonRock::randRangeFrom(int a, int b) { return (int)(zrand() % (uint32_t)((1 - a) + b)) + a; }
// @0x56bbc
void MoonRock::update() {
    angle = angle + _spinSpeed * FlxG::elapsed();
    if (_maxFallSpeed < velocity.y) velocity = CGPointMake(velocity.x, _maxFallSpeed);
    FlxSprite::update();
}
// @0x56cdc
void MoonRock::bounce() {
    int b = _bounces;
    _bounces = b - 1;
    if (0 < b) {
        velocity = CGPointMake(velocity.x, -229.5f);
        _spinSpeed = (float)randRangeFrom(-110, 110);
        FlxG::quake()->start(0.01f, 0.1f);
        FlxG::play("enemyTopple.wav");
    }
}
// @0x56dc8
void MoonRock::reset() {
    setY(-getHeight() - (float)randRangeFrom(0, 0x78));
    for (;;) {
        setX((float)randRangeFrom(0, (int)((float)FlxG::width() - getWidth())));
        double c = (double)(FlxG::width() / 2);
        if (!((double)x > c + (double)getWidth() * -1.5)) break;   // asm 0x56efc: vcmpe; ble -> exit on <=
        if (!((double)x < c + (double)getWidth() * 0.5)) break;
    }
    _toppling = false;
    _maxFallSpeed = (float)randRangeFrom(300, 0x1fe);
    _spinSpeed = (float)randRangeFrom(-110, 110);
    exists = true;
    velocity = CGPointZero;
    _bounces = 1;
    acceleration = CGPointMake(0, 240.0f);
}

// ================================================================ UFO
// @0x3a3c0
UFO* UFO::initWithGame(PlayState* game) {
    FlxSprite::initWithX(-300.0f, (float)(FlxG::height() - 0x136), std::string());
    loadGraphic("img_UFO.png", true, false, 0x80, 0x3e, true);
    addAnimation("normal", {0, 1}, 40.0f);
    addAnimation("charged", {3, 4, 5}, 40.0f);
    _chargeTimer = -1.0f;
    _strikeTimer = 0;
    _direction = 1;
    _emerged = false;
    exists = false;
    _game = game;
    _lightningSprite = (new FlxSprite)->init();
    _lightningSprite->loadGraphic("new_litBolt.png", true, false, 0x1e, 0x133, true);
    _lightningSprite->addAnimation("strike", {0, 7, 1, 2, 3, 4, 5, 6}, 8.0f, false);
    _lightningSprite->exists = false;
    return this;
}
// @0x3a630
void UFO::update() {
    if ((float)(FlxG::width() / 2) < x) _emerged = true;
    if (_chargeTimer <= 0.0f) {
        play("normal");
        if (!FlxG::playing("UFOnormal.wav")) {
            FlxG::stop("UFOangry.wav");
            FlxG::play("UFOnormal.wav", 0.06f, true);
        }
        _strikeTimer = 0.3f;
    } else {
        play("charged");
        if (!FlxG::playing("UFOangry.wav")) {
            FlxG::stop("UFOnormal.wav");
            FlxG::play("UFOangry.wav", 0.06f, true);
        }
        _chargeTimer = _chargeTimer - FlxG::elapsed();
        _strikeTimer = _strikeTimer - FlxG::elapsed();
        if (_strikeTimer <= 0.0f) {
            strike();
            _strikeTimer = (float)zrandom() * 4.656612873077393e-10f * 1.2f + 0.3f;
        }
    }
    x = x + FlxG::elapsed() * 240.0f * (float)_direction;
    if (x < (float)FlxG::width() - frameWidth + -5.0f || _direction != 1) {
        if (x <= 5.0f && _direction == -1 && 0.0f < _chargeTimer) _direction = 1;
    } else if (0.0f < _chargeTimer) {
        _direction = -1;
    } else if (x <= 5.0f && _direction == -1 && 0.0f < _chargeTimer) {
        _direction = 1;
    }
    if (_emerged) {
        if (!(x <= (float)FlxG::width() && -frameWidth <= x)) {
            FlxG::stop("UFOnormal.wav");
            kill();
        }
    }
    if (_lightningSprite->finished) _lightningSprite->exists = false;
    _lightningSprite->update();
    FlxSprite::update();
}
// @0x3aab4
void UFO::charge() { _chargeTimer = (0.0f < _chargeTimer) ? _chargeTimer + 2.5f : 5.0f; }
// @0x3aae4
void UFO::strike() {
    _lightningSprite->setX(x + getWidth() * 0.5f - _lightningSprite->getWidth() * 0.5f);
    _lightningSprite->setY(y + getHeight() * 0.5f);
    _lightningSprite->exists = true;
    _lightningSprite->play("strike", true);
    _game->addLightningAtX(x + getWidth() * 0.5f, y + getHeight() * 0.5f, 32.0f);
    FlxG::play("lightningStrike.wav", 0.06f);
}
// @0x3acac
void UFO::render() {
    if (_lightningSprite->exists) {
        if (enh().reduceFlashing) {  // ENHANCEMENT reduceFlashing: draw the bolt at 30 % (render only)
            float a = _lightningSprite->alpha();
            _lightningSprite->setAlpha(a * 0.3f);
            _lightningSprite->render();
            _lightningSprite->setAlpha(a);
        } else {
            _lightningSprite->render();
        }
    }
    FlxSprite::render();
}
// @0x3ad14
void UFO::stopSound() {
    FlxG::stop("UFOnormal.wav");
    FlxG::stop("UFOangry.wav");
}

// ================================================================ BGManager
static const char* const kTimeOfDay[4] = {"day.png", "lateDay.png", "sunset.png", "night.png"};  // 0x261aa4
static const float kBgLvlTime[4] = {115.0f, 85.0f, 53.0f, 0.0f};  // 0x1f7cec

// - (id) initWithGame:(PlayState *)game   @0x23950
BGManager* BGManager::initWithGame(PlayState* game) {
    FlxGroup::init();
    const int W = FlxG::width(), H = FlxG::height();
    SUN_BEGIN_X = 328.0f;
    SUN_BEGIN_Y = (float)(H - 0x154);
    SUN_END_X = 80.0f;
    SUN_END_Y = (float)(H - 0xad);
    MOON_X = 250.0f;
    MOON_Y = (float)(H - 0x163);
    _game = game;
    _foreground = (new FlxGroup)->init();
    _lvl = 0;
    _lvlTimer = 115.0f;
    _moonTremble = 3;
    _nightState = 0;
    _stateTime = 0;
    _totalTime = 0;
    auto* s = (new FlxSprite)->init();
    for (int i = 0; i < 4; i++) {
        std::string tod = kTimeOfDay[i];
        s->loadGraphic("sky_" + tod);
        s->setX((float)(W / 2) + s->getWidth() * -0.5f);
        s->setY((float)H - s->getHeight());
        _sky[i] = s;
        if (i < 2) {
            s = (new FlxSprite)->init();
            s->loadGraphic("sun_" + tod);
            s->setX(SUN_BEGIN_X);
            s->setY(SUN_BEGIN_Y);
            if (i == 1) s->setAlpha(0.0f);
            _sun[i] = s;
        }
        s = (new FlxSprite)->init();
        s->loadGraphic("zig4_" + tod);
        s->setX(115.0f);
        s->setY((float)(H - 0x8e));
        _zig4[i] = s;
        s = (new FlxSprite)->init();
        s->loadGraphic("clouds4_" + tod);
        s->setX(0.0f);
        s->setY((float)(H - 0x80));
        _cloud4[i] = s;
        s = (new FlxSprite)->init();
        s->loadGraphic("zig3_" + tod);
        s->setX(10.0f);
        s->setY((float)(H - 0x96));
        _zig3[i] = s;
        s = (new FlxSprite)->init();
        s->loadGraphic("clouds3_" + tod);
        s->setX(0.0f);
        s->setY((float)(H - 0x73));
        _cloud3[i] = s;
        s = (new FlxSprite)->init();
        s->loadGraphic("zig2_" + tod);
        s->setX(390.0f);
        s->setY((float)(H - 0x82));
        _zig2[i] = s;
        s = (new FlxSprite)->init();
        s->loadGraphic("clouds2_" + tod);
        s->setX(0.0f);
        s->setY((float)(H - 0x54));
        _cloud2[i] = s;
        s = (new FlxSprite)->init();
    }
    _stars = s;
    _stars->loadGraphic("stars.png");
    _stars->setX((float)(W / 2) - _stars->getWidth() * 0.5f);
    _stars->setY((float)H - _stars->getHeight());
    _stars->setAlpha(0);
    add(_stars);

    _moon = (new FlxSprite)->init();
    _moon->loadGraphic("img_moonSheet.png", true, false, 0xb7, 0xb7, true);
    _moon->setX(MOON_X);
    _moon->setY(MOON_Y);
    _moon->setAlpha(0);
    _moon->addAnimation("idle", {10});
    _moon->addAnimation("crack", {0, 1, 2, 3}, 2.0f, false);
    _moon->addAnimation("glow", {4, 5, 6, 7, 6, 5, 4, 5, 6, 7, 6, 5, 4}, 5.0f, true);
    _moon->addAnimation("explode", {4, 5, 6, 7, 0xb, 0xc, 0xd, 0xe, 0xf}, 5.0f, false);
    _moon->addAnimation("flicker", {8, 9}, 20.0f, true);
    add(_moon);
    _moon->play("idle");

    _smallExpEmitter = (new SmallExpEmitter)->initWithLifetime(5.0f);
    _smallExpEmitter->exists = false;
    _smallExpEmitter->setX(_moon->x + _moon->frameWidth * 0.5f);
    _smallExpEmitter->setY(_moon->y + _moon->frameHeight * 0.5f);
    add(_smallExpEmitter);

    _mainZig = (new FlxSprite)->init();
    _mainZig->loadGraphic("mainZig.png");
    _mainZig->setX((float)(W / 2) - _mainZig->getWidth() * 0.5f);
    _mainZig->setY((float)H - _mainZig->getHeight());

    _mainCloud = (new FlxSprite)->init();
    _mainCloud->loadGraphic("mainClouds.png");
    _mainCloud->setX(0);
    _mainCloud->setY((float)(H - 0x26));
    _foreground->add(_mainCloud);

    _screenFill = (new FlxSprite)->initWithXY(0.0f, 0.0f);
    _screenFill->createGraphicWithParam1(W, H, 0xffffffff, true);
    _screenFill->setAlpha(0);
    _screenFill->exists = false;
    return this;
}

static inline bool animIs(FlxSprite* s, const char* n) { return s->_curAnim && s->_curAnim->name == n; }

// - (void) update   @0x248f0
void BGManager::update() {
    if (_lvl < 3) {
        _lvlTimer = _lvlTimer - FlxG::elapsed();
        if (_lvlTimer <= 0.0f) {
            _lvl = _lvl + 1;
            _lvlTimer = kBgLvlTime[_lvl];
            if (_lvl == 3) {
                _screenFill->exists = true;
                _stateTime = 0;
            }
        }
    }
    if (_lvl < 2) {
        _totalTime = _totalTime + FlxG::elapsed();
        float t = _totalTime / 200.0f;
        float sx = (SUN_END_X - SUN_BEGIN_X) * t + SUN_BEGIN_X;
        float sy = (SUN_END_Y - SUN_BEGIN_Y) * t + SUN_BEGIN_Y;
        for (int i = 0; i < 2; i++) {
            _sun[i]->setX(sx);
            _sun[i]->setY(sy);
        }
    }
    _mainCloud->setX(_mainCloud->x + FlxG::elapsed() * 240.0f);
    if (0.0f <= _mainCloud->x) _mainCloud->setX(_mainCloud->x + _mainCloud->getWidth() * -0.5f);
    for (int i = 0; i < 4; i++) {
        _cloud2[i]->setX(_cloud2[i]->x + FlxG::elapsed() * 130.0f);
        if (0.0f <= _cloud2[i]->x) _cloud2[i]->setX(_cloud2[i]->x + -75.0f);
        _cloud3[i]->setX(_cloud3[i]->x + FlxG::elapsed() * 40.0f);
        if (0.0f <= _cloud3[i]->x) _cloud3[i]->setX(_cloud3[i]->x + -59.0f);
        _cloud4[i]->setX(_cloud4[i]->x + FlxG::elapsed() * 10.0f);
        if (0.0f <= _cloud4[i]->x) _cloud4[i]->setX(_cloud4[i]->x + -62.0f);
    }
    if (_lvl == 3) {
        if (_stars->alpha() < 1.0f || _moon->alpha() < 1.0f) {
            _stars->setAlpha(_stars->alpha() + FlxG::elapsed() * 0.2f);
            _moon->setAlpha(_moon->alpha() + FlxG::elapsed() * 0.2f);
        }
        _stateTime = _stateTime + FlxG::elapsed();
        switch (_nightState) {
            case 0:
                if (75.0f < _stateTime) {
                    _stateTime = 0.0f;
                    _nightState = 1;
                }
                break;
            case 1:
                _stars->setAlpha(_stateTime / -10.0f + 1.0f);
                if (_stars->alpha() <= 0.0f) {
                    _stars->exists = false;
                    _stateTime = 0;
                    _nightState = 2;
                    _moon->play("crack");
                }
                break;
            case 2:
                if (animIs(_moon, "crack")) {
                    _moon->setX(_moon->x + (float)_moonTremble);
                    _moonTremble = -_moonTremble;
                } else if (animIs(_moon, "flicker")) {
                    double d = (double)FlxG::elapsed() * 0.4;
                    _screenFill->setAlpha((float)((double)_screenFill->alpha() + d));
                    FlxG::quake()->start(0.005f, 0.1f);
                    if (1.0f <= _screenFill->alpha()) {
                        _moon->exists = false;
                        _stateTime = 0;
                        _nightState = 3;
                        _game->killAllEnemies();
                    }
                }
                if (_moon->finished) {
                    if (animIs(_moon, "crack")) {
                        _moon->play("glow");
                    } else if (animIs(_moon, "glow")) {
                        _moon->play("explode");
                        _smallExpEmitter->exists = true;
                        FlxG::play("moonPrejac.wav");
                    } else if (animIs(_moon, "explode")) {
                        _moon->play("flicker");
                        FlxG::play("MoonExplodes.wav");
                    }
                }
                break;
            case 3:
                if (2.0f < _stateTime) {
                    _stateTime = 0.0f;
                    _nightState = 4;
                }
                break;
            case 4:
                if (_screenFill->exists && 0.0f < _screenFill->alpha()) {
                    double e = (double)FlxG::elapsed();
                    _screenFill->setAlpha((float)((double)_screenFill->alpha() + e * -0.6));
                    if (_screenFill->alpha() <= 0.0f) _screenFill->exists = false;
                }
                break;
            default: break;
        }
    }
    _foreground->update();
    FlxGroup::update();
}

// Cross-fade helper used by -render for every layer: draw current level, then the previous one
// on top while it fades out (rate per second).
static void renderLayer(FlxSprite* const layer[4], int lvl, float rate) {
    layer[lvl]->render();
    if (0 < lvl && 0.0f < layer[lvl - 1]->alpha()) {
        layer[lvl - 1]->render();
        layer[lvl - 1]->setAlpha(layer[lvl - 1]->alpha() + FlxG::elapsed() * rate);
    }
}

// - (void) render   @0x2548c
void BGManager::render() {
    renderLayer(_sky, _lvl, -0.2f);
    if (_lvl == 2) {
        if (0.0f < _sun[1]->alpha()) {
            _sun[1]->setAlpha(_sun[1]->alpha() + FlxG::elapsed() * -0.2f);
            _sun[1]->render();
        }
    } else if (_lvl == 1) {
        if (_sun[1]->alpha() < 1.0f) {
            _sun[1]->setAlpha(_sun[1]->alpha() + FlxG::elapsed() * 0.2f);
            _sun[0]->render();
        }
        _sun[1]->render();
    } else if (_lvl == 0) {
        _sun[0]->render();
    }
    renderLayer(_zig4, _lvl, -0.2f);
    renderLayer(_cloud4, _lvl, -0.2f);
    renderLayer(_zig3, _lvl, -0.2f);
    renderLayer(_cloud3, _lvl, -0.2f);
    renderLayer(_zig2, _lvl, -0.2f);
    renderLayer(_cloud2, _lvl, -0.2f);
    _mainZig->render();
    FlxGroup::render();
}

// - (void) renderForeground   @0x25c60
void BGManager::renderForeground() {
    _foreground->render();
    if (!_screenFill->exists) return;
    if (enh().reduceFlashing) {  // ENHANCEMENT reduceFlashing: white-out at 30 % (render only; logic reads _alpha)
        float a = _screenFill->alpha();
        _screenFill->setAlpha(a * 0.3f);
        _screenFill->render();
        _screenFill->setAlpha(a);
        return;
    }
    _screenFill->render();
}
