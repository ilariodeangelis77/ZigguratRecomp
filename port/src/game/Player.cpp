// Player, PlayerBullet, EnemyBullet, Explosion.
// Reconstructed from ZiGGURAT (iOS) — see Gameplay.h for conventions.
#include <cmath>
#include <cstdlib>

#include "game/Gameplay.h"

// Tables from __const. Index -1 (chargeLevel == -1) reads the preceding word, so each table
// starts one entry early and is accessed through a pointer offset by one.
static const float kSpinRaw[7] = {570.0f, 10.0f, 100.0f, 300.0f, 600.0f, 900.0f, 1200.0f};  // 0x1f7d24..
static const float* const kSpin = kSpinRaw + 1;
static const float kScaleRaw[7] = {1200.0f, 0.8f, 1.0f, 1.25f, 1.5f, 1.75f, 2.5f};  // 0x1f7d3c..
static const float* const kScale = kScaleRaw + 1;
static const float kBulletSpeedRaw[7] = {1.401298464324817e-45f, 30.0f, 140.0f, 200.0f, 270.0f, 360.0f, 570.0f};  // 0x1f7d0c..
static const float* const kBulletSpeed = kBulletSpeedRaw + 1;
static const int kGunBob[6] = {0, 1, 1, 2, 1, 1};  // 0x1f7cf8, by hero animation frame

// ================================================================ Player
// - (id) initWithBulletManager:(FlxGroup *)bm game:(PlayState *)game   @0x2a66c
Player* Player::initWithBulletManager(FlxGroup* bullets, PlayState* game) {
    FlxGroup::init();
    _bulletManager = bullets;
    _game = game;
    _arcSlide = 0;
    _chargeLevel = -1;
    _reticleAngle = 180.0f;
    touchPivot = CGPointZero;
    _bulletSpeed = 0;
    _shotsFired = 0;
    _aimGuide = _game->_aimGuide;
    const int W = FlxG::width(), H = FlxG::height();

    _heroSprite = (new FlxSprite)->init();
    _heroSprite->loadGraphic("new_Hero.png", true, true, 0x18, 0x24, true);
    _heroSprite->offset = CGPointMake(5, 0);
    _heroSprite->setWidth(14.0f);
    _heroSprite->setX((float)(W / 2) + -7.0f);
    _heroSprite->setY((float)(H - 0xa0) - _heroSprite->getHeight());
    _heroSprite->setFacing(LEFT);
    _heroSprite->addAnimation("bounce", {0, 1, 2, 3, 4, 5}, 8.0f);

    _hitbox = (new FlxSprite)->init();
    _hitbox->setWidth(_heroSprite->getWidth());
    _hitbox->setHeight(_heroSprite->getHeight());
    _hitbox->setX(_heroSprite->x);
    _hitbox->setY(_heroSprite->y);
    add(_hitbox);
    add(_heroSprite);

    _gunSprite = (new FlxSprite)->init();
    _gunSprite->loadGraphic("new_Gun.png", false, true, 0x16, 8, true);
    _gunSprite->rotationOrigin = CGPointMake(3.0f, 3.0f);
    _gunSprite->setX((float)((double)(W / 2) + (double)_gunSprite->getWidth() * -0.75));
    _gunSprite->setY((float)(H - 0xa0) - _gunSprite->getHeight() + -10.0f);
    _gunSprite->solid = false;
    add(_gunSprite);

    _heroFront = (new FlxSprite)->init();
    _heroFront->loadGraphic("new_HeroFront.png", true, true, 0x18, 0x24, true);
    _heroFront->setX((float)(W / 2) + _heroFront->getWidth() * -0.5f);
    _heroFront->setY((float)(H - 0xa0) - _heroFront->getHeight());
    _heroFront->addAnimation("bounce", {0, 1, 2, 3, 4, 5}, 8.0f);
    add(_heroFront);

    _reticle = (new FlxSprite)->init();
    _reticle->loadGraphic("img_Reticle.png", true, false, 0xf, 0xf, true);
    _reticle->addAnimation("normal", {0});
    _reticle->addAnimation("flashing", {1, 2}, 20.0f);
    _reticle->solid = false;
    _reticle->setX(-100.0f);
    _reticle->setY(-100.0f);
    _reticle->visible = false;
    if (_game->ControlScheme() == 0) add(_reticle);

    _pivotSprite = (new FlxSprite)->init();
    _pivotSprite->loadGraphic("img_pivot.png");
    _pivotSprite->visible = false;
    add(_pivotSprite);

    _fireButton = (new FlxSprite)->init();  // never added to the group (unused leftover)
    _fireButton->createGraphicWithWidth(0x28, 0x28);
    _fireButton->visible = true;
    _fireButton->setX(224.0f);
    _fireButton->setY(160.0f);
    _fireButton->setWidth(40.0f);
    _fireButton->setHeight(40.0f);

    arcDots = (new FlxGroup)->init();
    for (int i = 0; i < 0x3c; i++) {
        auto* dot = (new FlxSprite)->init();
        dot->loadGraphic("img_arcDot.png");
        dot->visible = false;
        arcDots->add(dot);
    }
    add(arcDots);

    _chargingShot = (new FlxSprite)->initWithXY(0.0f, 0.0f);
    _chargingShot->loadGraphic("img_BulletPulse.png", true, false, 0x13, 0x13, true);
    _chargingShot->origin = CGPointZero;
    _chargingShot->scale = CGPointMake(kScale[_chargeLevel], kScale[_chargeLevel]);
    _chargingShot->rotationOrigin = CGPointMake(width * 0.5f, height * 0.5f);  // Player's own (zero) size
    _chargingShot->setWidth(kScale[_chargeLevel] * 11.0f);
    _chargingShot->setHeight(kScale[_chargeLevel] * 11.0f);
    _chargingShot->offset = CGPointMake(kScale[_chargeLevel] * 4.0f, kScale[_chargeLevel] * 4.0f);
    _chargingShot->visible = false;
    _chargingShot->addAnimation("pulse", {0, 1, 2, 3, 4, 5}, 15.0f);
    add(_chargingShot);
    updateGraphics();
    return this;
}

static float distInt(float dx, float dy) {
    int d = (int)(long long)sqrtf(dx * dx + dy * dy);
    return (float)(d < 0 ? -d : d);
}

// - (void) update   @0x2b2f4
void Player::update() {
    int lvl = _chargeLevel;
    float e = FlxG::elapsed();
    float spin = kSpin[lvl] * e;
    if (_heroSprite->facing() != RIGHT) spin = -spin;
    _chargingShot->angle = _chargingShot->angle + spin;
    if (-1 < _chargeLevel && !FlxG::playing("gunChargeUp.wav") && !FlxG::playing("gunChargeSustain.wav"))
        FlxG::play("gunChargeSustain.wav", 0.15f, true);
    _heroSprite->play("bounce");
    _heroFront->play("bounce");
    _chargingShot->play("pulse");
    FlxTouches* T = FlxG::touches();
    if (T->touching) _bulletSpeed = getBulletSpeed();
    const int W = FlxG::width(), H = FlxG::height();
    for (auto* m : arcDots->members) m->visible = false;
    const int cxi = W / 2, cyi = H - 0xa0;

    _fireDown = false;
    if (T->touching) {
        _fireDown = true;
        if (_chargeLevel != -1) _reticleAngle = getReticleAngleFromPoint(T->touchPoint());
    }
    if (_game->ControlScheme() == 0) {
        vectorX = (float)cos((double)_reticleAngle * 0.017453292519943295);
        vectorY = (float)sin((double)_reticleAngle * 0.017453292519943295);
    }
    if (_chargeLevel == -1) {
        if (!_fireDown || _fireDownLast) {
            if (_fireDown && _fireDownLast && _game->ControlScheme() != 0) {
                CGPoint tp = T->touchPoint();
                float d = distInt(touchPivot.x - tp.x, touchPivot.y - tp.y);
                if (20.0f < d || (_game->ControlScheme() == 1 && _game->TimedCharge() == 1)) {
                    _chargeLevel = 0;
                    _stateTime = 0.04f;
                    _chIter = 1;
                    updateChargingSize();
                }
            }
        } else {
            if (_game->ControlScheme() == 0) {
                _chargeLevel = 0;
                _stateTime = 0.04f;
                _chIter = 1;
                updateChargingSize();
            }
            touchPivot = T->touchPoint();
            _pivotSprite->setX(touchPivot.x - _pivotSprite->getWidth() * 0.5f);
            _pivotSprite->setY(touchPivot.y - _pivotSprite->getHeight() * 0.5f);
            if (_game->ControlScheme() == 1) {
                float d = distInt(touchPivot.x - (float)(W / 2), touchPivot.y - (float)(H / 2));
                if (50.0f < d) _fireDown = false;
            }
        }
    }
    if (!T->touching && _fireDown) _fireDown = false;
    const float cx = (float)cxi, cy = (float)cyi;

    if (_chargeLevel == -1) {
        if (_game->ControlScheme() == 2 && _pivotSprite->visible) _pivotSprite->visible = false;
    } else {
        if (!FlxG::playing("gunChargeUp.wav") && !FlxG::playing("gunChargeSustain.wav")) FlxG::play("gunChargeUp.wav", 0.15f);
        if (_aimGuide) drawBulletArc();
        if (_game->ControlScheme() == 2 && !_pivotSprite->visible) _pivotSprite->visible = true;
    }

    // Charge state machine (jump table @0x2bcd4)
    switch (_chargeLevel) {
        case 0:
            _chargingShot->setAlpha(_stateTime / 0.2f);
            goto level1;
        case 1:
        level1:
            _stateTime = _stateTime + FlxG::elapsed();
            if (_stateTime <= 0.24f) break;
            _stateTime = 0.0f;
            if (_chIter == 1) _chargeLevel = _chargeLevel + 1;
            else if (_chIter == -1) _chIter = 1;
            updateChargingSize();
            break;
        case 2:
        case 4:
            _stateTime = _stateTime + FlxG::elapsed();
            if (_stateTime <= 0.24f) break;
            _stateTime = 0.0f;
            _chargeLevel = _chargeLevel + _chIter;
            updateChargingSize();
            break;
        case 3:
            _stateTime = _stateTime + FlxG::elapsed();
            if (_stateTime <= 0.24f || _chIter < 1) break;
            _stateTime = 0.0f;
            _chargeLevel = _chargeLevel + _chIter;
            updateChargingSize();
            break;
        case 5:
            _stateTime = _stateTime + FlxG::elapsed();
            if (_stateTime <= 0.24f) break;
            _stateTime = 0.0f;
            _chIter = -1;
            _chargeLevel = _chargeLevel - 1;
            updateChargingSize();
            break;
        default: break;
    }

    if (!_fireDown && _fireDownLast && _chargeLevel != -1) {
        fire();
        _fireDown = false;
        _chargeLevel = -1;
        _chargingShot->scale = CGPointMake(0.8f, 0.8f);
    }

    _reticle->setX(cx + vectorX * 60.0f - _reticle->frameWidth * 0.5f);
    _reticle->setY(cy + vectorY * 60.0f - _heroSprite->getHeight() * 0.5f - _reticle->frameHeight * 0.5f);
    float s11 = kScale[_chargeLevel] * 11.0f;
    _chargingShot->setX(cx + vectorX * 28.0f + s11 * 0.5f + -10.0f);
    float sy = cy + vectorY * 28.0f - _heroSprite->getHeight() * 0.5f + 4.0f;
    _chargingShot->setY(sy + (kScale[_chargeLevel] * 11.0f) * 0.5f + -12.0f);
    updateGraphics();
    FlxGroup::update();
    _fireDownLast = _fireDown;
}

// - (float) getReticleAngleFromPoint:(CGPoint)p   @0x2c290
float Player::getReticleAngleFromPoint(CGPoint p) {
    (void)p;
    FlxTouches* T = FlxG::touches();
    if (_game->ControlScheme() == 0) {
        float tx = T->touchPoint().x;
        return tx / (float)FlxG::width() * 270.0f + 135.0f;
    }
    float px, py;
    if (_game->ControlScheme() == 2) {
        px = touchPivot.x;
        py = touchPivot.y;
    } else {
        px = (float)(FlxG::width() / 2);
        py = (float)(FlxG::height() - 0xa0) + height * -0.5f;
    }
    float dx = px - T->touchPoint().x;
    float dy = py - T->touchPoint().y;
    if (dx <= 10.0f && -10.0f <= dx && dy <= 10.0f && -10.0f <= dy) return _reticleAngle;
    float d = sqrtf(dx * dx + dy * dy);
    vectorX = dx / d;
    vectorY = dy / d;
    if (vectorY == 0.0f) vectorY = 0.01f;
    float a = (float)(atan((double)(fabsf(vectorY) / fabsf(vectorX))) * 57.29577951308232);
    if (0.0f <= vectorX) {
    q4:
        if (vectorX <= 0.0f || 0.0f <= vectorY) return a;
        return 360.0f - a;
    }
    if (vectorY <= 0.0f) {
        if (vectorY < 0.0f) return a + 180.0f;
        goto q4;
    }
    return 180.0f - a;
}

// - (void) updateGraphics   @0x2c5bc
void Player::updateGraphics() {
    bool back = (90.0f <= _reticleAngle && _reticleAngle <= 270.0f);
    unsigned int face = back ? LEFT : RIGHT;
    _heroSprite->setFacing(face);
    _heroFront->setFacing(face);
    _gunSprite->setFacing(face);
    unsigned f = _heroSprite->frame();
    _gunSprite->setY((float)(FlxG::height() - 0xa0) - _gunSprite->getHeight() + -10.0f - (float)kGunBob[f < 6 ? f : 0]);
    float cx = (float)(FlxG::width() / 2);
    if (back) {
        _gunSprite->setX(cx - _gunSprite->getWidth());
        _gunSprite->angle = 180.0f - _reticleAngle;
    } else {
        _gunSprite->setX(cx);
        _gunSprite->angle = _reticleAngle;
    }
    _chargingShot->visible = (_chargeLevel != -1);
    _reticle->play(_chargeLevel == 5 ? "flashing" : "normal");
}

// - (void) updateChargingSize   @0x2c844
void Player::updateChargingSize() {
    float s = kScale[_chargeLevel];
    _chargingShot->origin = CGPointZero;
    _chargingShot->scale = CGPointMake(s, s);
    _chargingShot->rotationOrigin = CGPointMake(width * 0.5f, height * 0.5f);
    _chargingShot->setWidth(s * 11.0f);
    _chargingShot->setHeight(s * 11.0f);
    _chargingShot->offset = CGPointMake(s * 4.0f, s * 4.0f);
}

// - (void) drawBulletArc   @0x2c998
void Player::drawBulletArc() {
    const int W = FlxG::width(), H = FlxG::height();
    float vxs = vectorX * _bulletSpeed;
    float vys = vectorY * _bulletSpeed;
    float slide = _arcSlide;
    float px = (float)(W / 2) + vectorX * 28.0f + vxs * slide;
    float py = (float)(H - 0xa0) - height * 0.5f + vectorY * 28.0f - _heroSprite->getHeight() * 0.5f + 4.0f + vys * slide;
    float vy = vys + slide * 300.0f;
    const float dx = vxs * 0.1f;
    for (auto* m : arcDots->members) {
        auto* dot = (FlxSprite*)m;
        dot->visible = true;
        dot->setX(px - dot->getWidth() * 0.5f);
        px = px + dx;
        dot->setY(py - dot->getHeight() * 0.5f);
        float step = vy * 0.1f;
        vy = vy + 30.0f;
        py = py + step;
    }
    _arcSlide = _arcSlide + FlxG::elapsed() * 0.5999999642372131f;
    if (0.1f <= _arcSlide) _arcSlide = 0.0f;
}

// - (void) fire   @0x2cc7c
void Player::fire() {
    const int W = FlxG::width(), H = FlxG::height();
    float bx = (float)(W / 2) + vectorX * 28.0f;
    float by = (float)(H - 0xa0) + vectorY * 28.0f + _heroSprite->getHeight() * -0.5f + 4.0f;
    int lvl = _chargeLevel;
    auto* b = (new PlayerBullet)->initWithX(bx, by, vectorX, vectorY, _bulletSpeed, kScale[lvl], lvl, kSpin[lvl]);
    _bulletManager->add(b);
    FlxG::play("shoot.wav", 0.14f);
    FlxG::stop("gunChargeUp.wav");
    FlxG::stop("gunChargeSustain.wav");
    _shotsFired = _shotsFired + 1;
}

// - (void) die   @0x2ce70
void Player::die() {
    _killed = true;
    FlxG::stop("gunChargeUp.wav");
    FlxG::stop("gunChargeSustain.wav");
}

// - (void) cancelChargeForced:(BOOL)forced   @0x2ced8
void Player::cancelChargeForced(BOOL forced) {
    if (_chargeLevel < 3 || forced == 1) {
        _chargeLevel = -1;
        _game->addPBulletExplosionWithX(_chargingShot->x + _chargingShot->getWidth() * 0.5f,
                                         _chargingShot->y + _chargingShot->getHeight() * 0.5f, 1.0f);
        _fireDown = false;
        _fireDownLast = false;
        FlxG::stop("gunChargeUp.wav");
        FlxG::stop("gunChargeSustain.wav");
    }
}

// - (float) getBulletSpeed   @0x2d024
float Player::getBulletSpeed() {
    if (_game->ControlScheme() == 0 || _game->TimedCharge() == 1) {
        if (_game->SmoothCharge() == 0) return kBulletSpeed[_chargeLevel];
        float t = _stateTime / 0.24f;
        double d = (double)t;
        if (t > 1.0f) d = 1.0;
        if (_chIter < 1) d = 1.0 - d;
        int r = (int)(long long)((((double)_chargeLevel + d) / 6.0) * 570.0);
        return (float)(r < 0 ? -r : r);
    }
    float px = 0, py = 0;
    if (_game->ControlScheme() == 2) {
        px = touchPivot.x;
        py = touchPivot.y;
    } else if (_game->ControlScheme() == 1) {
        px = (float)(FlxG::width() / 2);
        py = (float)(FlxG::height() - 0xa0) + height * -0.5f;
    }
    CGPoint tp = FlxG::touches()->touchPoint();
    float d = distInt(px - tp.x, py - tp.y);
    float ratio = d / ((float)FlxG::width() * 0.33f);
    if (0.0f <= ratio) {
        if (1.0f < ratio) ratio = 1.0f;
    } else {
        ratio = 0.01f;
    }
    return ratio * 280.0f;
}

// ================================================================ PlayerBullet
// @0x2d3d4
PlayerBullet* PlayerBullet::initWithX(float X, float Y, float vx, float vy, float speed, float bulletScale, int level, float spin) {
    Bullet::initWithBulletXY(X, Y);
    _lvl = level;
    _remainingBounces = 1;
    _rotationSpeed = spin;
    velocity.x = vx * speed;
    velocity.y = vy * speed;
    loadGraphic("new_PlayerBullet.png");
    acceleration.y = 300.0f;
    origin = CGPointZero;
    scale = CGPointMake(bulletScale, bulletScale);
    rotationOrigin = CGPointMake(width * 0.5f, height * 0.5f);
    width = bulletScale * 11.0f;   // direct ivar writes in the original (no -setWidth:)
    height = bulletScale * 11.0f;
    offset = CGPointMake(bulletScale * 4.0f, bulletScale * 4.0f);
    x = x - width * 0.5f;
    y = y - height * 0.5f;
    return this;
}
// @0x2d5c8
void PlayerBullet::update() {
    float d = _rotationSpeed * FlxG::elapsed();
    angle = (velocity.x <= 0.0f) ? angle - d : angle + d;
    FlxSprite::update();
}
// @0x2d688
BOOL PlayerBullet::bounce() {
    int r = _remainingBounces;
    _remainingBounces = r - 1;
    return 0 < r;
}

// ================================================================ EnemyBullet
// - init   @0x269d0
EnemyBullet* EnemyBullet::initEnemyBullet() {
    Bullet::initWithBulletXY(0.0f, 0.0f);
    loadGraphic("new_EnemyBullet.png");
    _hitbox = (new FlxSprite)->init();
    origin = CGPointZero;
    rotationOrigin = CGPointMake(width * 0.5f, height * 0.5f);
    width = 11.0f;
    height = 11.0f;
    _hitbox->setWidth(width);
    _hitbox->setHeight(height);
    _hitbox->createGraphicWithWidth((unsigned)(int)width, (unsigned)(int)height);
    exists = false;
    return this;
}
// - resetWithX:y:vectorX:vectorY:bulletSpeed:   @0x26b64
void EnemyBullet::resetWithX(float X, float Y, float vx, float vy, float speed) {
    setX(X);
    setY(Y);
    velocity.x = vx * speed;
    velocity.y = vy * speed;
    exists = true;
}
// @0x26bdc
void EnemyBullet::update() {
    _hitbox->setX(x);
    _hitbox->setY(y);
    float d = FlxG::elapsed() * 800.0f;
    angle = (velocity.x <= 0.0f) ? angle - d : angle + d;
    FlxSprite::update();
}

// ================================================================ Explosion
// @0x26d1c
Explosion* Explosion::initExplosion() {
    FlxGroup::init();
    _sprite = (new FlxSprite)->init();
    _sprite->loadGraphic("new_Explosion.png", true, false, 0x48, 0x48);
    add(_sprite);
    return this;
}
// @0x26dcc
void Explosion::resetWithX(float X, float Y, float r) {
    setX(X);
    setY(Y);
    _sprite->setX(X);
    _sprite->setY(Y);
    _sprite->setX(_sprite->x - _sprite->getWidth() * 0.5f);
    _sprite->setY(_sprite->y - _sprite->getHeight() * 0.5f);
    float d = r + r;
    _sprite->scale = CGPointMake(d / _sprite->getWidth(), d / _sprite->getHeight());
    _radius = r;
    _aliveTime = 0;
    exists = true;
    _sprite->play("explode", true);
}
// @0x26f8c
void Explosion::update() {
    if (_sprite->finished) exists = false;
    _sprite->update();
}
