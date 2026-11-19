// Alien, YellowAlien, RedAlien, OrangeAlien, PurpleAlien, BigAlien, BigPurpleAlien.
// Reconstructed from ZiGGURAT (iOS) — see Gameplay.h for conventions.
#include <cmath>
#include <cstdlib>
#include <random>

#include "game/Enhancements.h"
#include "game/Gameplay.h"

// ---------------------------------------------------------------- RNG (arc4random / random)
static std::mt19937& rng() {
    static std::mt19937 g = [] {
        const char* s = SDL_getenv("ZIGGURAT_SEED");
        return std::mt19937(s ? (uint32_t)strtoul(s, nullptr, 10) : std::random_device{}());
    }();
    return g;
}
uint32_t zrand() { return rng()(); }
long zrandom() { return (long)(rng()() & 0x7fffffffu); }

// Velocity sign used everywhere in the original: (facing == LEFT) ? +v : -v
static inline float dirLeft(FlxSprite* s, float v) { return s->facing() == LEFT ? v : -v; }

// Tables from __const (0x1f7cc4 / 0x1f7cd8 / 0x1f7ed8 / 0x1f7eec)
static const float kHeadRadius[5] = {20, 40, 60, 80, 100};
static const float kLvlMul[5] = {1.0f, 1.2f, 1.4f, 1.6f, 1.8f};
static const float kYellowHeadRadius[5] = {60, 70, 80, 90, 100};
static const float kYellowLvlMul[5] = {1.0f, 1.2f, 1.4f, 1.6f, 1.8f};

static const char* const kAlienHeads[5] = {"img_AlienHead_size01.png", "img_AlienHead_size02.png", "img_AlienHead_size03.png",
                                          "img_AlienHead_size04.png", "img_AlienHead_size05.png"};
static const char* const kYellowHeads[5] = {"img_YellowAlienHead_size01.png", "img_YellowAlienHead_size02.png",
                                           "img_YellowAlienHead_size03.png", "img_YellowAlienHead_size04.png",
                                           "img_YellowAlienHead_size05.png"};
static const char* const kPurpleHeads[5] = {"img_PurpleAlienHead_size01.png", "img_PurpleAlienHead_size02.png",
                                           "img_PurpleAlienHead_size03.png", "img_PurpleAlienHead_size04.png",
                                           "img_PurpleAlienHead_size05.png"};
static const char* const kBigHeads[5] = {"img_AlienHead3x_size01.png", "img_AlienHead3x_size02.png", "img_AlienHead3x_size03.png",
                                        "img_AlienHead3x_size04.png", "img_AlienHead3x_size05.png"};
static const char* const kBigPurpleHeads[5] = {"img_BigPurpleAlienHead_size01.png", "img_BigPurpleAlienHead_size02.png",
                                              "img_BigPurpleAlienHead_size03.png", "img_BigPurpleAlienHead_size04.png",
                                              "img_BigPurpleAlienHead_size05.png"};

// ================================================================ Alien
// - (id) initWithGame:(PlayState *)game   @0x21cbc
Alien* Alien::initWithGame(PlayState* game) {
    FlxSprite::initWithX(0, 0, std::string());
    _game = game;
    loadGraphics();
    exists = false;
    addAnimation("descending", {4, 5}, 4.0f);
    addAnimation("ascending", {0, 1, 2}, 4.0f, false);
    addAnimation("exploding", {1, 0}, 40.0f);
    return this;
}

// Shared body of every -loadGraphics: body sheet, hit box, offset, and 5 head sizes whose
// rotationOrigin is (headW*0.5 + originX, headH + originY + bodyH*0.5).
void Alien::loadBodyAndHeads(const char* body, unsigned fw, unsigned fh, float w, float h, CGPoint off,
                             const char* const heads[5], float originX, float originY) {
    loadGraphic(body, true, true, fw, fh);
    setWidth(w);
    setHeight(h);
    offset = off;
    for (int i = 0; i < 5; i++) {
        auto* head = (new FlxSprite)->init();
        head->loadGraphic(heads[i], false, true);
        CGPoint ro;
        ro.x = head->getWidth() * 0.5f + originX;
        ro.y = head->getHeight() + originY + getHeight() * 0.5f;
        head->rotationOrigin = ro;
        _headSprites[i] = head;
    }
}

// - (void) loadGraphics   @0x21df0
void Alien::loadGraphics() {
    loadBodyAndHeads("img_AlienBody.png", 0x1c, 0x1e, 18.0f, 24.0f, CGPointMake(5, 4), kAlienHeads, 5.0f, -6.0f);
}

// - resetWithX:withY:withJumpPower:withMoveSpeed:withBulletSpeed:withFireTime:   @0x21f70
void Alien::resetWithX(float X, float Y, float jumpPower, float moveSpeed, float bulletSpeed, float fireTime) {
    setX(X);
    setY(Y);
    _jumpPower = jumpPower;
    _moveSpeed = moveSpeed;
    _bulletSpeed = bulletSpeed;
    _fireTime = fireTime;
    _shotTimer = fireTime;
    _state = 0;
    _headLvl = (int)(zrand() & 3);
    _headLvlTime = 0;
    _lvlIter = 1;
    _currentHead = _headSprites[0];
    acceleration.y = 100.0f;
    velocity.x = moveSpeed;
    angle = 0;
    exists = true;
    solid = true;
}

// - resetFloatyWithX:...   @0x220d8
void Alien::resetFloatyWithX(float X, float Y, float jumpPower, float moveSpeed, float bulletSpeed, float fireTime) {
    resetWithX(X, Y, jumpPower, moveSpeed, bulletSpeed, fireTime);
    acceleration.y = 30.0f;
}

// - (void) update   @0x22134
void Alien::update() {
    if (acceleration.y == 30.0f) {  // floaty blue aliens: jump power depends on height
        int bottom = (int)(y + getHeight());
        if (bottom < 0xa5) _jumpPower = 85.0f;
        else if (bottom < 0xc5) _jumpPower = 95.0f;
        else if (0xe4 < bottom) _jumpPower = bottom < 0x105 ? 115.0f : 122.0f;
        else _jumpPower = 105.0f;
    }
    switch (_state) {
        case 0: updateAlive(); break;
        case 1: updateExploding(); break;
        case 2: updateToppling(); break;
        case 3: updateStunned(); break;
        case 4: updateProne(); break;
        default: break;
    }
    FlxSprite::update();
}

// - (void) updateAlive   @0x2228c
void Alien::updateAlive() {
    updateHeadSize();
    float vy = velocity.y;
    if (vy < 0.0f) {
        play("ascending");
    } else if (vy > 0.0f) {
        play("descending");
        if (velocity.x == 0.0f) velocity.x = dirLeft(this, _moveSpeed);
    }
    if (onFloor) {
        velocity.y = -_jumpPower;
        velocity.x = dirLeft(this, _moveSpeed);
        FlxG::play("bounce.wav", 0.2f);
    }
    if (onAlien == 1) velocity.y = -30.0f;
    unsigned int face = LEFT;
    if (x + getWidth() < (float)FlxG::width()) {
        if (0.0f < x) goto shoot;
        velocity.x = _moveSpeed;
    } else {
        velocity.x = -_moveSpeed;
        face = RIGHT;
    }
    setFacing(face);
shoot:
    _shotTimer = _shotTimer - FlxG::elapsed();
    if (_shotTimer <= 0.0f) {
        _shotTimer = _fireTime;
        fireBullet();
    }
}

// - (void) updateExploding   @0x22594
void Alien::updateExploding() {
    _stateTime = _stateTime + FlxG::elapsed();
    acceleration = CGPointZero;
    velocity = CGPointZero;
    play("exploding");
    if (0.25f < _stateTime) {
        exists = false;
        _game->addExplosionWithX(x + getWidth() * 0.5f, y + getHeight() * 0.5f, _explosionRadius);
        addGibs();
    }
}

// - (void) addGibs   @0x2272c
void Alien::addGibs() { _game->addBlueGibsAtX(x, y); }

// - (void) updateToppling   @0x22780
void Alien::updateToppling() {
    angle = angle + (facing() == LEFT ? 700.0f : -700.0f) * FlxG::elapsed();
    play("exploding");
    if (!onFloor && y <= (float)FlxG::height()) return;
    exists = false;
    addGibs();
}

// - (void) updateStunned   @0x228b0
void Alien::updateStunned() {
    angle = angle + (facing() == LEFT ? 700.0f : -700.0f) * FlxG::elapsed();
    play("exploding");
    if (!onFloor) return;
    randomFrame();
    angle = 90.0f;
    _state = 4;
    velocity = CGPointZero;
    acceleration = CGPointZero;
    _stateTime = 0;
}

// - (void) updateProne   @0x229ec
void Alien::updateProne() {
    _stateTime = _stateTime + FlxG::elapsed();
    if (_stateTime <= 1.0f) return;
    angle = 0.0f;
    _state = 0;
    acceleration.y = 100.0f;
}

// - (void) render   @0x22a84
void Alien::render() {
    updateHeadPosition();
    FlxSprite::render();
    _currentHead->render();
}

// - (void) updateHeadSize   @0x22ad8
void Alien::updateHeadSize() {
    _headLvlTime = _headLvlTime + FlxG::elapsed();
    if (0.4f <= _headLvlTime) {
        int l = _headLvl;
        if (l == 0) {
            _lvlIter = 1;
            _headLvl = _headLvl + 1;
            _headLvlTime = 0;
        } else if ((unsigned)(l - 1) < 3) {
            _headLvlTime = 0.0f;
            _headLvl = _lvlIter + _headLvl;
        } else {
            if (l < 4) goto pos;
            _lvlIter = -1;
            _headLvl = _headLvl - 1;
            _headLvlTime = 0;
        }
        _currentHead = _headSprites[_headLvl];
    }
pos:
    updateHeadPosition();
}

// - (void) updateHeadPosition   @0x22c20
void Alien::updateHeadPosition() {
    _currentHead->setFacing((unsigned int)(-(int)facing()));
    float hx = x + getWidth() * 0.5f - _currentHead->frameWidth * 0.5f;
    _currentHead->setX(hx + (facing() == LEFT ? 5.0f : -5.0f));
    _currentHead->setY(y - _currentHead->frameHeight + 6.0f + -4.0f);
    float a = angle;
    if (facing() != RIGHT) a = 360.0f - a;
    _currentHead->angle = a;
}

// - (void) fireBullet   @0x22de0
void Alien::fireBullet() {
    float dx = (float)(FlxG::width() / 2) - x;
    float dy = ((float)(FlxG::height() - 0xa0) + -16.0f) - y;
    float d = sqrtf(dx * dx + dy * dy);
    _game->addEnemyBulletAtX(x + getWidth() * 0.5f, y, dx / d, dy / d, _bulletSpeed);
    FlxG::play("enemyShoot.wav", 0.8f);
}

// - (void) headshotByBulletWithLvl:(int)lvl   @0x22f3c
void Alien::headshotByBulletWithLvl(int lvl) {
    int i = lvl - 1;
    if (lvl == 0) i = 0;
    _explosionRadius = kHeadRadius[_headLvl] * kLvlMul[i];
    _state = 1;
    _stateTime = 1.0f;
    solid = false;
}

// - bodyshotWithVelocityX:y:   @0x22fcc
void Alien::bodyshotWithVelocity(float vx, float vy) {
    _state = 2;
    velocity.x = vx;
    velocity.y = vy;
    acceleration.y = 300.0f;
}

// - hitExplosionWithRadius:   @0x23018
void Alien::hitExplosionWithRadius(float r) {
    _explosionRadius = r * 0.5f;
    _state = 1;
    _stateTime = 0;
    solid = false;
}

// - stunWithVelocityX:y:   @0x23074
void Alien::stunWithVelocity(float vx, float vy) {
    (void)vy;
    _state = 3;
    velocity.x = vx * 0.25f;
    velocity.y = -75.0f;
    acceleration.y = 300.0f;
}

// - hitWithMoonRockWithVelocityX:y:   @0x230d4
void Alien::hitWithMoonRockWithVelocity(float vx, float vy) {
    _state = 3;
    velocity.x = vx;
    velocity.y = vy;
    acceleration.x = 0;
    acceleration.y = 300.0f;
}

// ================================================================ YellowAlien
// @0x3ad8c
Alien* YellowAlien::initWithGame(PlayState* game) {
    Alien::initWithGame(game);
    addAnimation("walk", {0, 1, 2, 1}, 4.0f);
    addAnimation("climb", {4});
    return this;
}
// @0x3ae38
void YellowAlien::loadGraphics() {
    loadBodyAndHeads("img_YellowAlienBody.png", 0x17, 0x1e, 18.0f, 24.0f, CGPointMake(5, 4), kYellowHeads, 5.0f, -6.0f);
}
// @0x3b164
void YellowAlien::resetWithXYMoveSpeed(float X, float Y, float ms) {
    Alien::resetWithX(X, Y, 0, ms, 0, 0);
    acceleration.y = 500.0f;
}
// @0x3afbc
void YellowAlien::updateAlive() {
    updateHeadSize();
    velocity.x = dirLeft(this, _moveSpeed);
    if (!onFloor) {
        acceleration.y = 500.0f;
    } else {
        play("walk");
        acceleration.y = 0;
    }
    unsigned int face = LEFT;
    if (x + getWidth() < (float)FlxG::width()) {
        if (0.0f < x) return;
        velocity.x = _moveSpeed;
    } else {
        velocity.x = -_moveSpeed;
        face = RIGHT;
    }
    setFacing(face);
}
// @0x3b1b0  (note: no "jump" animation exists; play() resets the frame counters like the original)
void YellowAlien::jump() {
    velocity.y = -175.0f;
    play("jump");
}
// @0x3b1e8
void YellowAlien::addGibs() { _game->addYellowGibsAtX(x, y); }
// @0x3b23c
void YellowAlien::headshotByBulletWithLvl(int lvl) {
    int i = lvl - 1;
    if (lvl == 0) i = 0;
    _explosionRadius = kYellowHeadRadius[_headLvl] * kYellowLvlMul[i];
    _state = 1;
    _stateTime = 1.0f;
    solid = false;
}

// ================================================================ RedAlien
// @0x57580
Alien* RedAlien::initWithGame(PlayState* game) {
    Alien::initWithGame(game);
    addAnimation("idle", {0});
    _tremble = 2;
    return this;
}
// @0x57600  ORIGINAL BUG kept: -setRotationOrigin: is sent to _currentHead, which is still nil during
// -loadGraphics, so the single head keeps rotationOrigin (0,0) and rotates about its origin.
void RedAlien::loadGraphics() {
    loadGraphic("img_RedAlienBody.png", true, true, 0x1c, 0x1e);
    setWidth(18.0f);
    setHeight(24.0f);
    offset = CGPointMake(5, 4);
    auto* head = (new FlxSprite)->init();
    head->loadGraphic("img_RedAlienHead.png", false, true);
    _headSprites[0] = head;
}
// @0x57778
void RedAlien::updateAlive() {
    updateHeadPosition();
    if (!onFloor) {
        acceleration.y = 200.0f;
        float vy = velocity.y;
        if (vy < 0.0f) play("ascending");
        else if (vy > 0.0f) play("descending");
    } else {
        play("idle");
        acceleration.y = 0;
        velocity.x = 0;
        acceleration.x = 0;
        _pounceTimer = _pounceTimer - FlxG::elapsed();
        if (_pounceTimer <= 0.0f) pounce();
    }
    unsigned int face;
    if (x + getWidth() < (float)FlxG::width()) {
        if (0.0f < x) return;
        velocity.x = 30.0f;
        face = LEFT;
    } else {
        velocity.x = -30.0f;
        face = RIGHT;
    }
    setFacing(face);
}
// @0x5799c
void RedAlien::addGibs() { _game->addRedGibsAtX(x, y); }
// @0x579f0
void RedAlien::resetWithXYMoveSpeed(float X, float Y, float ms) {
    Alien::resetWithX(X, Y, 0, ms, 0, 0);
    acceleration.y = 200.0f;
    _pounceTimer = 1.6f;
}
// @0x57a50
void RedAlien::render() {
    if ((double)_pounceTimer <= 0.4 && 0.15 < (double)_pounceTimer) {
        setX(x + (float)_tremble);
        _tremble = -_tremble;
    }
    Alien::render();
}
// @0x57b18  (_pounceTimer is not reset: every landing pounces again, as in the original)
void RedAlien::pounce() {
    if (enh().fixRedPounce) _pounceTimer = 1.6f;  // ENHANCEMENT fixRedPounce: wait again before the next pounce
    acceleration.y = 160.0f;
    velocity.x = facing() == LEFT ? 90.0f : -90.0f;
    velocity.y = -300.0f;
}

// ================================================================ OrangeAlien
// @0x39650
Alien* OrangeAlien::initWithGame(PlayState* game) {
    Alien::initWithGame(game);
    addAnimation("idle", {0});
    _orangeState = 0;
    _orangeStateTime = 0;
    _vulnerable = false;
    return this;
}
// @0x396ec  (same _currentHead rotationOrigin bug as RedAlien)
void OrangeAlien::loadGraphics() {
    loadGraphic("img_OrangeAlienBody.png", true, true, 0x1c, 0x1e);
    setWidth(18.0f);
    setHeight(24.0f);
    offset = CGPointMake(5, 4);
    auto* head = (new FlxSprite)->init();
    head->loadGraphic("img_OrangeAlienHead.png", false, true);
    _headSprites[0] = head;
    _shield = (new FlxSprite)->init();
    _shield->loadGraphic("img_shield.png");
}
// @0x398a8
void OrangeAlien::updateAlive() {
    if (_vulnerable) {
        float e = FlxG::elapsed();
        _shield->setAlpha(_shield->alpha() + e * -4.0f);
        _shield->scale = CGPointMake(_shield->scale.x + 0.15f, _shield->scale.y + 0.15f);
    }
    updateHeadPosition();
    _shield->update();
    _orangeStateTime = _orangeStateTime + FlxG::elapsed();
    switch (_orangeState) {
        case 0: {
            if (x + getWidth() < (float)FlxG::width()) {
                if (x <= 0.0f) {
                    velocity.x = 40.0f;
                    setFacing(LEFT);
                }
            } else {
                velocity.x = -40.0f;
                setFacing(RIGHT);
            }
            acceleration.y = 200.0f;
            play("descending");
            if (!onFloor) return;
            play("idle");
            _orangeState = 1;
            _orangeStateTime = 0;
            velocity = CGPointZero;
            break;
        }
        case 1:
            if (1.0f <= _orangeStateTime / 1.6f) {
                play("ascending");
                _orangeState = 2;
                _orangeStateTime = 0;
                startingX = (int)x;
                startingY = (int)y;
            }
            break;
        case 2: {
            float t = _orangeStateTime;
            int h = FlxG::height();
            int sy = startingY;
            float e = FlxG::elapsed();
            float t2 = t + t;
            setX(x + e * (facing() == LEFT ? 30.0f : -30.0f));
            setY((float)((h - 0xe6) - sy) * t2 + (float)sy);
            if (t2 < 1.0f) return;
            _orangeStateTime = 0;
            _orangeState = 3;
            acceleration = CGPointZero;
            velocity = CGPointZero;
            break;
        }
        case 3: {
            float t = _orangeStateTime * 0.25f;
            float a = t * 8.0f, b = t * 15.0f;
            double dy = sin((double)b) * -110.0 * (double)FlxG::elapsed();
            double sa = sin((double)a);
            setY((float)((double)y + dy));
            double dx = sa * 30.0 * (double)FlxG::elapsed();
            dx = dx * (facing() == LEFT ? 1.0 : -1.0);
            setX((float)((double)x + dx));
            if (0.65 < (double)t && !FlxG::playing("shield.wav")) FlxG::play("shield.wav", 0.4f);
            if (1.0f <= t) {
                FlxG::play("shieldOff.wav", 0.4f);
                _orangeStateTime = 0;
                _orangeState = 4;
                play("descending");
                _vulnerable = true;
            }
            break;
        }
        case 4:
            if (1.0f <= _orangeStateTime / 0.4f) {
                _orangeStateTime = 0.0f;
                _orangeState = 5;
            }
            break;
        case 5: {
            float t = _orangeStateTime / 3.0f;
            float tx = (float)(FlxG::width() / 2) + getWidth() * -0.5f;
            float nx = (tx - x) * t + x;
            float ty = (float)(FlxG::height() - 0xaa);
            float ny = (ty - y) * t + y;
            setX(nx);
            setY(ny);
            break;
        }
        default: break;
    }
}
// @0x3a14c
void OrangeAlien::addGibs() { _game->addOrangeGibsAtX(x, y); }
// @0x3a1a0
void OrangeAlien::resetWithXYMoveSpeed(float X, float Y, float ms) {
    Alien::resetWithX(X, Y, 0, ms, 0, 0);
    acceleration.y = 200.0f;
    _orangeStateTime = 0;
    _orangeState = 0;
    _vulnerable = false;
    _shield->setAlpha(1.0f);
    _shield->scale = CGPointMake(1, 1);
}
// @0x3a254
void OrangeAlien::render() {
    Alien::render();
    _shield->setX(x - _shield->getWidth() / 3.0f);
    _shield->setY(y - _shield->getHeight() / 3.0f);
    _shield->flicker(1);
    _shield->render();
}

// ================================================================ PurpleAlien
// @0x5f280
void PurpleAlien::loadGraphics() {
    loadBodyAndHeads("img_PurpleAlienBody.png", 0x1c, 0x1e, 18.0f, 24.0f, CGPointMake(5, 4), kPurpleHeads, 5.0f, -6.0f);
}
// @0x5f404
void PurpleAlien::update() {
    Alien::update();
    if (velocity.x != 0.0f) return;
    if (facing() == LEFT) setFacing(RIGHT);
    else if (facing() == RIGHT) setFacing(LEFT);
    velocity.x = dirLeft(this, _moveSpeed);
}
// @0x5f514
void PurpleAlien::resetWithXYMoveSpeed(float X, float Y, float ms) {
    Alien::resetWithX(X, Y, 450.0f, ms, 0, 0);
    acceleration.y = 500.0f;
}
// @0x5f564
void PurpleAlien::addGibs() { _game->addPurpleGibsAtX(x, y); }

// ================================================================ BigAlien
// @0x25d18
Alien* BigAlien::initWithGame(PlayState* game) {
    Alien::initWithGame(game);
    _hp = getMaxHP();
    _miniExplosionTimer = 0.2f;
    _goddamnHitbox = (new FlxSprite)->init();
    _goddamnHitbox->setWidth(getWidth());
    _goddamnHitbox->setHeight(getHeight());
    return this;
}
// @0x25e0c
void BigAlien::render() {
    _goddamnHitbox->setX(x);
    _goddamnHitbox->setY(y);
    _goddamnHitbox->render();  // invisible: its 8x8 fill happened while _alpha was 0 (original quirk)
    Alien::render();
}
// @0x25ea0
void BigAlien::loadGraphics() {
    loadBodyAndHeads("img_AlienBody3x.png", 0x54, 0x5a, 54.0f, 72.0f, CGPointMake(19, 9), kBigHeads, 15.0f, -18.0f);
    fixed = true;
}
// @0x26038
void BigAlien::updateHeadPosition() {
    _currentHead->setFacing((unsigned int)(-(int)facing()));
    float hx = x + getWidth() * 0.5f - _currentHead->frameWidth * 0.5f;
    _currentHead->setX(hx + (facing() == LEFT ? 15.0f : -15.0f));
    _currentHead->setY(y - _currentHead->frameHeight + 18.0f + -9.0f);
    float a = angle;
    if (facing() != RIGHT) a = 360.0f - a;
    _currentHead->angle = a;
}
// @0x261f4
void BigAlien::headshotByBulletWithLvl(int lvl) {
    FlxG::play("giantEnemyHit.wav", 0.6f);
    int dmg = 0;
    switch (lvl) {
        case 0: case 1: dmg = 1; break;
        case 2: dmg = 2; break;
        case 3: dmg = 4; break;
        case 4: dmg = 8; break;
        case 5: dmg = 0x10; break;
        default: break;
    }
    _hp = _hp - dmg;
    if (_hp < 1) {
        _explosionRadius = 50.0f;
        flicker(2.5f);
        acceleration = CGPointZero;
        velocity.x = 0;
        velocity.y = 30.0f;
        _state = 1;
        _stateTime = 1.0f;
        solid = false;
        return;
    }
    flicker(0.2f);
    acceleration = CGPointZero;
    velocity.y = 0;
    float k = getKnockbackSpeed();
    velocity.x = facing() == RIGHT ? k : -k;
    _stateTime = 0;
    _state = 3;
}
// @0x263c0
void BigAlien::updateAlive() {
    if (onFloor) {
        FlxG::quake()->start(0.025f, 0.1f);
        FlxG::play("giantEnemyBounce.wav", 0.4f);
    }
    Alien::updateAlive();
}
// @0x26468
void BigAlien::updateExploding() {
    _miniExplosionTimer = _miniExplosionTimer - FlxG::elapsed();
    _stateTime = _stateTime + FlxG::elapsed();
    float e = FlxG::elapsed();
    angle = angle + (facing() == RIGHT ? 20.0f : -20.0f) * e;
    play("exploding");
    if (_miniExplosionTimer <= 0.0f) {
        _miniExplosionTimer = 0.2f;
        uint32_t r1 = zrand();
        uint32_t w = (uint32_t)(int)getWidth();
        uint32_t r2 = zrand();
        uint32_t rx = w ? r1 % w : 0;
        uint32_t h = (uint32_t)(int)getHeight();
        uint32_t ry = h ? r2 % h : 0;
        _game->addExplosionWithX(x + (float)rx, y + (float)ry, _explosionRadius);
    }
    if (2.5f < _stateTime) {
        exists = false;
        _game->addExplosionWithX(x + getWidth() * 0.5f, y + getHeight() * 0.5f, _explosionRadius);
    }
}
// @0x2677c
void BigAlien::updateStunned() {
    _stateTime = _stateTime + FlxG::elapsed();
    if (!(x < (float)FlxG::width() && frameWidth * -0.5f < x)) velocity.x = 0;
    if (0.2f < _stateTime) {
        acceleration.y = 100.0f;
        _state = 0;
        velocity.x = 0;
    }
}
// @0x268bc
void BigAlien::resetWithX(float X, float Y, float jumpPower, float moveSpeed, float bulletSpeed, float fireTime) {
    Alien::resetWithX(X, Y, jumpPower, moveSpeed, bulletSpeed, fireTime);
    _hp = getMaxHP();
}

// ================================================================ BigPurpleAlien
// @0x5f5bc
void BigPurpleAlien::loadGraphics() {
    loadBodyAndHeads("img_BigPurpleAlienBody.png", 0x54, 0x5a, 54.0f, 72.0f, CGPointMake(19, 9), kBigPurpleHeads, 15.0f, -18.0f);
    fixed = true;
}
// @0x5f754  (plays gameOver.wav as its landing thud, then BigAlien's own quake + bounce sound)
void BigPurpleAlien::updateAlive() {
    if (onFloor) {
        FlxG::quake()->start(0.025f, 0.1f);
        FlxG::play("gameOver.wav", 0.6f);
    }
    BigAlien::updateAlive();
}
