// ZiGGURAT gameplay classes, reconstructed from the iOS binary (Zig2, armv7, v1.0.1).
// Every method carries its original selector and address. Ivar names are the original ones.
//
// Decompilation notes that apply throughout:
//  - LEFT == 0, RIGHT == 1 (flixel-ios). Sprites in this game face LEFT while moving +x.
//  - Wherever the original read a per-level table with index -1 (chargeLevel == -1), the table
//    below includes the preceding words of the binary's __const section, so indexing matches.
//  - arc4random()/random() are replaced by zrand()/zrandom() (std::mt19937, seed ZIGGURAT_SEED).
#pragma once
#include <cstdint>
#include <vector>

#include "engine/Flixel.h"
#include "game/Shared.h"

class PlayState;

uint32_t zrand();     // arc4random()
long zrandom();       // random(): 0 .. 2^31-1

// ---------------------------------------------------------------- bullets
class Bullet : public FlxSprite {
public:
    // - (id) initWithX:(float)X y:(float)Y   @0x26968
    Bullet* initWithBulletXY(float X, float Y) { FlxSprite::initWithXY(X, Y); return this; }
};

class EnemyBullet : public Bullet {
public:
    FlxSprite* _hitbox = nullptr;
    EnemyBullet* initEnemyBullet();  // - init @0x269d0
    void resetWithX(float x, float y, float vx, float vy, float speed);  // @0x26b64
    void update() override;  // @0x26bdc
    FlxSprite* hitbox() { return _hitbox; }
};

class PlayerBullet : public Bullet {
public:
    int _lvl = 0;
    int _remainingBounces = 0;
    float _rotationSpeed = 0;
    // - initWithX:y:vectorX:vectorY:bulletSpeed:bulletScale:level:spinSpeed:  @0x2d3d4
    PlayerBullet* initWithX(float x, float y, float vx, float vy, float speed, float bulletScale, int level, float spin);
    using FlxSprite::initWithX;
    void update() override;  // @0x2d5c8
    int level() { return _lvl; }
    BOOL bounce();  // @0x2d688
};

// ---------------------------------------------------------------- effects
class Explosion : public FlxGroup {  // (unused by PlayState in v1.0.1, ported for completeness)
public:
    float _radius = 0, _aliveTime = 0;
    FlxSprite* _sprite = nullptr;
    Explosion* initExplosion();  // @0x26d1c
    void resetWithX(float x, float y, float radius);  // @0x26dcc
    void update() override;  // @0x26f8c
    float radius() { return _radius; }
};

class GibCluster : public FlxGroup {
public:
    FlxSprite *_head = nullptr, *_Larm = nullptr, *_Rarm = nullptr, *_torso = nullptr, *_Rleg = nullptr, *_Lleg = nullptr;
    int _gibSpinRate[6] = {};
    float _timer = 0;
    GibCluster* initWithColor(const std::string& color);  // @0x4c370
    void resetWithXY(float x, float y);  // @0x4c72c
    void update() override;  // @0x4c8fc
};

class SmallExpEmitter : public FlxGroup {
public:
    BOOL _emitting = false;
    float _lifetime = 0;
    SmallExpEmitter* initWithLifetime(float lifetime);  // @0x5709c
    void update() override;  // @0x571c8
    int randRangeFrom(int a, int b);
    void stopEmitting() { _emitting = false; }
};

class MoonRock : public FlxSprite {
public:
    int _bounces = 0;
    float _spinSpeed = 0, _maxFallSpeed = 0;
    BOOL _toppling = false;
    MoonRock* initMoonRock();  // @0x56b10
    void update() override;  // @0x56bbc
    void bounce();  // @0x56cdc
    void reset();  // @0x56dc8
    int randRangeFrom(int a, int b);
};

class UFO : public FlxSprite {
public:
    float _chargeTimer = 0, _strikeTimer = 0;
    int _direction = 0;
    BOOL _emerged = false;
    PlayState* _game = nullptr;
    FlxSprite* _lightningSprite = nullptr;
    UFO* initWithGame(PlayState* game);  // @0x3a3c0
    void update() override;  // @0x3a630
    void charge();  // @0x3aab4
    void strike();  // @0x3aae4
    void render() override;  // @0x3acac
    void stopSound();  // @0x3ad14
};

// ---------------------------------------------------------------- aliens
class Alien : public FlxSprite {
public:
    float _jumpPower = 0, _moveSpeed = 0, _bulletSpeed = 0, _fireTime = 0, _shotTimer = 0, _explosionRadius = 0, _stateTime = 0;
    int _state = 0;  // 0 alive, 1 exploding, 2 toppling, 3 stunned, 4 prone
    FlxSprite* _headSprites[5] = {};
    FlxSprite* _currentHead = nullptr;
    int _headLvl = 0, _lvlIter = 0;
    float _headLvlTime = 0;
    BOOL onAlien = false;
    PlayState* _game = nullptr;

    virtual Alien* initWithGame(PlayState* game);  // @0x21cbc
    virtual void loadGraphics();                   // @0x21df0
    virtual void resetWithX(float x, float y, float jumpPower, float moveSpeed, float bulletSpeed, float fireTime);  // @0x21f70
    void resetFloatyWithX(float x, float y, float jumpPower, float moveSpeed, float bulletSpeed, float fireTime);  // @0x220d8
    // Subclasses' - resetWithX:withY:withMoveSpeed:
    virtual void resetWithXYMoveSpeed(float x, float y, float moveSpeed) { resetWithX(x, y, 0, moveSpeed, 0, 0); }
    void update() override;           // @0x22134
    virtual void updateAlive();       // @0x2228c
    virtual void updateExploding();   // @0x22594
    virtual void addGibs();           // @0x2272c
    virtual void updateToppling();    // @0x22780
    virtual void updateStunned();     // @0x228b0
    virtual void updateProne();       // @0x229ec
    void render() override;           // @0x22a84
    void updateHeadSize();            // @0x22ad8
    virtual void updateHeadPosition();  // @0x22c20
    virtual void fireBullet();        // @0x22de0
    virtual void headshotByBulletWithLvl(int lvl);  // @0x22f3c
    void bodyshotWithVelocity(float vx, float vy);  // @0x22fcc
    void hitExplosionWithRadius(float r);           // @0x23018
    void stunWithVelocity(float vx, float vy);      // @0x23074
    void hitWithMoonRockWithVelocity(float vx, float vy);  // @0x230d4
    int state() { return _state; }
    FlxSprite* headSprite() { return _currentHead; }
    // OrangeAlien only (PlayState sends -_vulnerable to orange aliens)
    virtual BOOL vulnerable() { return false; }
    virtual void jump() {}
protected:
    void loadBodyAndHeads(const char* body, unsigned fw, unsigned fh, float w, float h, CGPoint off,
                          const char* const heads[5], float originX, float originY);
};

class YellowAlien : public Alien {
public:
    Alien* initWithGame(PlayState* game) override;  // @0x3ad8c
    void loadGraphics() override;                   // @0x3ae38
    void resetWithXYMoveSpeed(float x, float y, float ms) override;  // @0x3b164
    void updateAlive() override;                    // @0x3afbc
    void jump() override;                           // @0x3b1b0
    void addGibs() override;                        // @0x3b1e8
    void headshotByBulletWithLvl(int lvl) override; // @0x3b23c
};

class RedAlien : public Alien {
public:
    float _pounceTimer = 0;
    int _tremble = 0;
    Alien* initWithGame(PlayState* game) override;  // @0x57580
    void loadGraphics() override;                   // @0x57600
    void updateAlive() override;                    // @0x57778
    void addGibs() override;                        // @0x5799c
    void resetWithXYMoveSpeed(float x, float y, float ms) override;  // @0x579f0
    void render() override;                         // @0x57a50
    void pounce();                                  // @0x57b18
};

class OrangeAlien : public Alien {
public:
    int _orangeState = 0;
    float _orangeStateTime = 0;
    int startingX = 0, startingY = 0;
    BOOL _vulnerable = false;
    FlxSprite* _shield = nullptr;
    Alien* initWithGame(PlayState* game) override;  // @0x39650
    void loadGraphics() override;                   // @0x396ec
    void updateAlive() override;                    // @0x398a8
    void addGibs() override;                        // @0x3a14c
    void resetWithXYMoveSpeed(float x, float y, float ms) override;  // @0x3a1a0
    void render() override;                         // @0x3a254
    BOOL vulnerable() override { return _vulnerable; }
};

class PurpleAlien : public Alien {
public:
    void loadGraphics() override;                   // @0x5f280
    void update() override;                         // @0x5f404
    void resetWithXYMoveSpeed(float x, float y, float ms) override;  // @0x5f514
    void addGibs() override;                        // @0x5f564
    void fireBullet() override {}                   // @0x5f5b8 (empty)
};

class BigAlien : public Alien {
public:
    int _hp = 0;
    float _miniExplosionTimer = 0;
    FlxSprite* _goddamnHitbox = nullptr;
    Alien* initWithGame(PlayState* game) override;  // @0x25d18
    FlxSprite* hitbox() { return _goddamnHitbox; }
    void render() override;                         // @0x25e0c
    void loadGraphics() override;                   // @0x25ea0
    void updateHeadPosition() override;             // @0x26038
    void headshotByBulletWithLvl(int lvl) override; // @0x261f4
    void fireBullet() override {}                   // @0x263bc (empty)
    void updateAlive() override;                    // @0x263c0
    void updateExploding() override;                // @0x26468
    void updateStunned() override;                  // @0x2677c
    virtual float getKnockbackSpeed() { return 120.0f; }  // @0x268b0
    virtual int getMaxHP() { return 48; }                 // @0x268b8
    void resetWithX(float x, float y, float jumpPower, float moveSpeed, float bulletSpeed, float fireTime) override;  // @0x268bc
};

class BigPurpleAlien : public BigAlien {
public:
    void loadGraphics() override;                   // @0x5f5bc
    void updateAlive() override;                    // @0x5f754
    float getKnockbackSpeed() override { return 30.0f; }  // @0x5f7fc
    int getMaxHP() override { return 250; }               // @0x5f804
};

// ---------------------------------------------------------------- player
class Player : public FlxGroup {
public:
    FlxSprite *_heroSprite = nullptr, *_gunSprite = nullptr, *_heroFront = nullptr, *_reticle = nullptr, *_pivotSprite = nullptr;
    FlxGroup* _bulletManager = nullptr;
    FlxSprite* _chargingShot = nullptr;
    FlxSprite* _hitbox = nullptr;
    PlayState* _game = nullptr;
    FlxGroup* arcDots = nullptr;
    float _arcSlide = 0;
    FlxSprite* _fireButton = nullptr;
    BOOL _fireDown = false, _fireDownLast = false;
    int _chargeLevel = 0;
    float _stateTime = 0;
    int _chIter = 0;
    float _reticleAngle = 0;
    BOOL _killed = false;
    CGPoint touchPivot;
    float _bulletSpeed = 0;
    unsigned int _shotsFired = 0;
    BOOL _aimGuide = false;
    float vectorX = 0, vectorY = 0;

    Player* initWithBulletManager(FlxGroup* bullets, PlayState* game);  // @0x2a66c
    void update() override;               // @0x2b2f4
    float getReticleAngleFromPoint(CGPoint p);  // @0x2c290
    void updateGraphics();                // @0x2c5bc
    void updateChargingSize();            // @0x2c844
    void drawBulletArc();                 // @0x2c998
    void fire();                          // @0x2cc7c
    void die();                           // @0x2ce70
    void cancelCharge() { cancelChargeForced(false); }  // @0x2cec0
    void cancelChargeForced(BOOL forced); // @0x2ced8
    float getBulletSpeed();               // @0x2d024
    FlxSprite* hitbox() { return _hitbox; }
    FlxSprite* heroSprite() { return _heroSprite; }
    FlxSprite* heroFront() { return _heroFront; }
    FlxSprite* chargingShot() { return _chargingShot; }
    CGPoint shotVector() { return CGPointMake(vectorX * _bulletSpeed, vectorY * _bulletSpeed); }  // @0x2d33c
    unsigned int shotsFired() { return _shotsFired; }
};

// ---------------------------------------------------------------- background
class BGManager : public FlxGroup {
public:
    PlayState* _game = nullptr;
    FlxSprite* _sky[4] = {};
    FlxSprite* _stars = nullptr;
    FlxSprite* _moon = nullptr;
    FlxSprite* _sun[2] = {};
    FlxSprite *_zig4[4] = {}, *_cloud4[4] = {}, *_zig3[4] = {}, *_cloud3[4] = {}, *_zig2[4] = {}, *_cloud2[4] = {};
    FlxSprite *_mainZig = nullptr, *_mainCloud = nullptr;
    SmallExpEmitter* _smallExpEmitter = nullptr;
    FlxGroup* _foreground = nullptr;
    int _lvl = 0;
    float _lvlTimer = 0, _totalTime = 0;
    int _moonTremble = 0;
    FlxSprite* _screenFill = nullptr;
    int _nightState = 0;
    float _stateTime = 0;
    float SUN_BEGIN_X = 0, SUN_BEGIN_Y = 0, SUN_END_X = 0, SUN_END_Y = 0, MOON_X = 0, MOON_Y = 0;

    BGManager* initWithGame(PlayState* game);  // @0x23950
    void update() override;                    // @0x248f0
    void render() override;                    // @0x2548c
    void renderForeground();                   // @0x25c60
    void changeToLevel(int lvl) { _lvl = lvl - 1; }  // @0x25cb8
    float silhouettePercent() { return 0.0f; }       // @0x25ccc
};

// ---------------------------------------------------------------- play state
class PlayState : public FlxState {
public:
    Player* _player = nullptr;
    FlxGroup *_playerBullets = nullptr, *_pBulletExplosions = nullptr, *_eBulletExplosions = nullptr, *_enemyBullets = nullptr;
    FlxGroup* _enemies = nullptr;
    std::vector<Alien*> _topplingEnemies;  // NSMutableArray
    FlxGroup *_bigEnemies = nullptr, *_bigPurpleEnemies = nullptr, *_yellowEnemies = nullptr, *_redEnemies = nullptr,
             *_orangeEnemies = nullptr, *_purpleEnemies = nullptr, *_blockingBoxes = nullptr, *_explosions = nullptr;
    FlxGroup *_gibClustersBlue = nullptr, *_gibClustersYellow = nullptr, *_gibClustersRed = nullptr, *_gibClustersOrange = nullptr,
             *_gibClustersPurple = nullptr, *_moonRocks = nullptr;
    UFO* _UFO = nullptr;
    BGManager* _background = nullptr;
    int _controlScheme = 0;
    BOOL _smoothCharge = false, _timedCharge = false, _godMode = false, _aimGuide = false;
    float _UFOtimer = 0;
    BOOL _UFOappeared = false;
    float _bgLvlTimer = 0;
    int _currentBgLvl = 0;
    float _enemyLvlTimer = 0;
    int _currentEnemyLvl = 0;
    float _blueSpawnTimer = 0, _giantSpawnTimer = 0, _bigPurpleSpawnTimer = 0, _yellowSpawnTimer = 0, _redSpawnTimer = 0,
          _orangeSpawnTimer = 0, _purpleSpawnTimer = 0;
    FlxObject* _killer = nullptr;
    BOOL _gameOver = false;
    float _gameOverTime = 0;
    unsigned int _numKilled = 0;
    BOOL _silhouette = false;
    float _silhouetteTime = 0;
    unsigned int _consecutiveKills = 0, _highestConsecutiveKills = 0;
    FlxSprite* _silhouetteImage = nullptr;
    int _killerType = 0;
    unsigned int _blueKills = 0, _yellowKills = 0, _redKills = 0, _orangeKills = 0, _purpleKills = 0, _giantKills = 0,
                 _bigPurpleKills = 0, _missedShots = 0;
    float _gameTime = 0;
    unsigned int _ufoHits = 0, _headShots = 0, _shotsUntilUniverseEnd = 0;
    BOOL _moonRocksActive = false;
    float _moonRockDelayTimer = 0, _moonRockTimer = 0;

    PlayState* initWithScheme(int scheme, BOOL aimGuide, BOOL godMode);  // @0x2d6cc
    void create() override;              // @0x2da10
    void update() override;              // @0x2e8c4
    void render() override;              // @0x393b4
    void addBlockingBoxWithX(float x, float y, float w, float h);  // @0x2ed14
    void updateMoonRocks();              // @0x2eda0
    void updateEnemies();                // @0x2eed0
    void handleCollisions();             // @0x2f808
    void cleanUp();                      // @0x33e0c
    void hitPlayerWith(FlxObject* o);    // @0x34874
    void addExplosionWithX(float x, float y, float radius);        // @0x34974
    void addPBulletExplosionWithX(float x, float y, float radius); // @0x35074
    void addEBulletExplosionWithX(float x, float y, float radius); // @0x35238
    void addLightningAtX(float x, float y, float width);           // @0x353fc
    void addEnemyBulletAtX(float x, float y, float vx, float vy, float speed);  // @0x35b20
    void addBlueAlienAtX(float x, float y, float jump, float move, float bullet, float fire, BOOL floaty);  // @0x35c34
    void addBigAlienAtX(float x, float y, float jump, float move, float bullet, float fire);        // @0x35d64
    void addBigPurpleAlienAtX(float x, float y, float jump, float move, float bullet, float fire);  // @0x35e80
    void addYellowAlienAtX(float x, float y, float ms);  // @0x35f9c
    void addRedAlienAtX(float x, float y, float ms);     // @0x360a0
    void addOrangeAlienAtX(float x, float y, float ms);  // @0x361a4
    void addPurpleAlienAtX(float x, float y, float ms);  // @0x362a8
    void addBlueGibsAtX(float x, float y) { addGibs(_gibClustersBlue, x, y, "blue"); }
    void addYellowGibsAtX(float x, float y) { addGibs(_gibClustersYellow, x, y, "yellow"); }
    void addRedGibsAtX(float x, float y) { addGibs(_gibClustersRed, x, y, "red"); }
    void addOrangeGibsAtX(float x, float y) { addGibs(_gibClustersOrange, x, y, "orange"); }
    void addPurpleGibsAtX(float x, float y) { addGibs(_gibClustersPurple, x, y, "purple"); }
    void killAllEnemies();               // @0x3685c
    int ControlScheme() { return _controlScheme; }
    BOOL SmoothCharge() { return _smoothCharge; }
    BOOL TimedCharge() { return _timedCharge; }
    int randRangeFrom(int a, int b);     // @0x36ef8
    void updateStats();                  // @0x36f14
    void incrementConsecutiveKills();    // @0x392f8
    void clearConsecutiveKills();        // @0x39328
    float distanceBetweenPoint(CGPoint a, CGPoint b);  // @0x39388 (squared distance!)
private:
    void addGibs(FlxGroup* g, float x, float y, const char* color);
    void addExplosionSprite(FlxGroup* g, float x, float y, const char* capMsg);
};
