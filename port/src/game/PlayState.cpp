// PlayState — the ZiGGURAT game loop. Reconstructed from ZiGGURAT (iOS) — see Gameplay.h.
#include <algorithm>
#include "core/TestHooks.h"
#include <cmath>
#include <cstdlib>

#include "game/Enhancements.h"
#include "game/Gameplay.h"

// Difficulty tables, indexed by _currentEnemyLvl (0..7). __const 0x1f7d58..0x1f7ed4.
static const float kBlueSpawn[8] = {1.8f, 1.5f, 1.3f, 1.0f, 0.8f, 0.8f, 0.8f, 0.8f};
static const float kGiantSpawn[8] = {60, 45, 40, 35, 30, 27, 90, 27};
static const float kYellowSpawn[8] = {7, 7, 6, 6, 5, 4, 6, 4};
static const float kRedSpawn[8] = {14, 12, 12, 11, 10, 8, 12, 8};
static const float kOrangeSpawn[8] = {30, 27, 25, 24, 22, 22, 30, 22};
static const float kEnemyLvlTime[8] = {30, 30, 30, 20, 0, 90, 100, 0};
static const float kBlueMoveMin[8] = {9, 12, 15, 18, 24, 27, 15, 27};
static const float kBlueMoveMax[8] = {20, 28, 36, 44, 52, 64, 35, 64};
static const float kBlueBulletMin[8] = {18, 18, 20, 22, 28, 28, 20, 28};
static const float kBlueBulletMax[8] = {24, 27, 32, 36, 40, 44, 32, 44};
static const float kBlueFireMin[8] = {7.0f, 6.5f, 5.5f, 5.0f, 4.5f, 4.0f, 5.5f, 4.0f};
static const float kBlueFireMax[8] = {8.0f, 7.5f, 6.0f, 5.5f, 5.0f, 4.5f, 6.5f, 4.0f};

template <class T>
static inline T* as(FlxObject* o) { return static_cast<T*>(o); }

// ================================================================ factories (Shared.h)
FlxState* makePlayState(int scheme, bool aimGuide, bool godMode) {
    return (new PlayState)->initWithScheme(scheme, aimGuide, godMode);
}
// - initWithManagedObjectContext:  @0x2d6a4 -> initWithScheme:1 withAimGuide:NO withGodMode:NO
FlxState* makePlayStateDefault() { return (new PlayState)->initWithScheme(1, false, false); }

// ================================================================ init / create
// ENHANCEMENT showDeathSnapshot: frame captured when the player is hit.
static const plat::Texture* gDeathSnapshot = nullptr;

// - initWithScheme:withAimGuide:withGodMode:withManagedObjectContext:   @0x2d6cc
PlayState* PlayState::initWithScheme(int scheme, BOOL aimGuide, BOOL godMode) {
    FlxState::init();
    setBgColor(0xff68ace5);
    _blockingBoxes = (new FlxGroup)->init();
    _background = (new BGManager)->initWithGame(this);
    _gameOver = false;
    _gameOverTime = 0;
    _numKilled = 0;
    _consecutiveKills = 0;
    _highestConsecutiveKills = 0;
    _currentBgLvl = 0;
    _currentEnemyLvl = 0;
    _enemyLvlTimer = 30.0f;
    _UFOtimer = 125.0f;
    _UFOappeared = false;
    _blueSpawnTimer = 1.0f;
    _giantSpawnTimer = 1.0f;
    _bigPurpleSpawnTimer = 90.0f;
    _yellowSpawnTimer = kYellowSpawn[_currentEnemyLvl];
    _redSpawnTimer = kRedSpawn[_currentEnemyLvl];
    _orangeSpawnTimer = 1.0f;
    _purpleSpawnTimer = 32.0f;
    _controlScheme = scheme;
    if (zig::testEnv("ZIGGURAT_GODMODE")) godMode = true;  // PORT: test hook (original: hidden StartState toggle)
    if (enh().godMode) godMode = true;                    // ENHANCEMENT (practice): the original's hidden god mode
    _smoothCharge = true;
    _timedCharge = false;
    _godMode = godMode;
    _aimGuide = aimGuide;
    _blueKills = _yellowKills = _redKills = _orangeKills = _giantKills = _bigPurpleKills = _purpleKills = 0;
    _gameTime = 0;
    _ufoHits = 0;
    _headShots = 0;
    _missedShots = 0;
    _shotsUntilUniverseEnd = 0xffffffffu;
    _moonRocksActive = false;
    _moonRockTimer = 0;
    _moonRockDelayTimer = 10.0f;
    // ENHANCEMENT (practice): start at enemy level N (0..4; 5+ only follow the moon explosion).
    if (enh().startEnemyLevel > 0) {
        int lvl = enh().startEnemyLevel > 4 ? 4 : enh().startEnemyLevel;
        _currentEnemyLvl = lvl;
        _enemyLvlTimer = kEnemyLvlTime[lvl];
        _yellowSpawnTimer = kYellowSpawn[lvl];
        _redSpawnTimer = kRedSpawn[lvl];
    }
    return this;
}

// - (void) create   @0x2da10
void PlayState::create() {
    _playerBullets = (new FlxGroup)->init();
    _pBulletExplosions = (new FlxGroup)->init();
    _eBulletExplosions = (new FlxGroup)->init();
    _player = (new Player)->initWithBulletManager(_playerBullets, this);
    _enemyBullets = (new FlxGroup)->init();
    _enemies = (new FlxGroup)->init();
    _topplingEnemies.clear();
    _bigEnemies = (new FlxGroup)->init();
    _bigPurpleEnemies = (new FlxGroup)->init();
    _yellowEnemies = (new FlxGroup)->init();
    _redEnemies = (new FlxGroup)->init();
    _orangeEnemies = (new FlxGroup)->init();
    _purpleEnemies = (new FlxGroup)->init();
    _explosions = (new FlxGroup)->init();
    _gibClustersBlue = (new FlxGroup)->init();
    _gibClustersYellow = (new FlxGroup)->init();
    _gibClustersRed = (new FlxGroup)->init();
    _gibClustersOrange = (new FlxGroup)->init();
    _gibClustersPurple = (new FlxGroup)->init();
    _UFO = (new UFO)->initWithGame(this);
    _moonRocks = (new FlxGroup)->init();
    add(_gibClustersBlue);
    add(_gibClustersYellow);
    add(_gibClustersRed);
    add(_gibClustersOrange);
    add(_gibClustersPurple);
    add(_UFO);
    add(_player);
    add(_playerBullets);
    add(_enemies);
    add(_bigEnemies);
    add(_bigPurpleEnemies);
    add(_yellowEnemies);
    add(_redEnemies);
    add(_orangeEnemies);
    add(_purpleEnemies);
    add(_moonRocks);
    add(_enemyBullets);
    const int H = FlxG::height();
    addBlockingBoxWithX(224.0f, (float)(H - 0xa0), 32.0f, 32.0f);
    addBlockingBoxWithX(176.0f, (float)(H - 0x80), 128.0f, 32.0f);
    addBlockingBoxWithX(128.0f, (float)(H - 0x60), 224.0f, 32.0f);
    addBlockingBoxWithX(80.0f, (float)(H - 0x40), 320.0f, 32.0f);
    addBlockingBoxWithX(-42.0f, (float)(H - 0x20), 570.0f, 32.0f);
    add(_blockingBoxes);
    add(_eBulletExplosions);
    add(_pBulletExplosions);
    add(_explosions);
    // Pool sizes recovered from the interleaved loop counters at 0x2da10.
    for (int i = 0; i < 100; i++) _enemyBullets->add((new EnemyBullet)->initEnemyBullet());
    for (int i = 0; i < 20; i++) _enemies->add((new Alien)->initWithGame(this));
    for (int i = 0; i < 2; i++) _bigEnemies->add((new BigAlien)->initWithGame(this));
    _bigPurpleEnemies->add((new BigPurpleAlien)->initWithGame(this));
    for (int i = 0; i < 20; i++) _yellowEnemies->add((new YellowAlien)->initWithGame(this));
    for (int i = 0; i < 20; i++) _redEnemies->add((new RedAlien)->initWithGame(this));
    _orangeEnemies->add((new OrangeAlien)->initWithGame(this));
    for (int i = 0; i < 3; i++) _purpleEnemies->add((new PurpleAlien)->initWithGame(this));
    for (int i = 0; i < 5; i++) _gibClustersBlue->add((new GibCluster)->initWithColor("blue"));
    for (int i = 0; i < 5; i++) _gibClustersYellow->add((new GibCluster)->initWithColor("yellow"));
    for (int i = 0; i < 5; i++) _gibClustersRed->add((new GibCluster)->initWithColor("red"));
    for (int i = 0; i < 5; i++) _gibClustersOrange->add((new GibCluster)->initWithColor("orange"));
    for (int i = 0; i < 5; i++) _gibClustersPurple->add((new GibCluster)->initWithColor("purple"));
    for (int i = 0; i < 3; i++) _moonRocks->add((new MoonRock)->initMoonRock());
    for (int i = 0; i < 100; i++) {
        auto* s = (new FlxSprite)->init();
        s->loadGraphicWithParam1("new_Explosion.png", true, false, 0x48, 0x48);
        s->addAnimation("explode", {0, 1, 2, 3, 4, 5}, 13.0f, false);
        s->exists = false;
        _explosions->add(s);
    }
    for (int i = 0; i < 100; i++) {
        auto* s = (new FlxSprite)->init();
        s->loadGraphicWithParam1("img_BulletExplode.png", true, false, 0x30, 0x30);
        s->addAnimation("explode", {0, 1, 2, 3}, 15.0f, false);
        s->exists = false;
        _pBulletExplosions->add(s);
    }
    for (int i = 0; i < 100; i++) {
        auto* s = (new FlxSprite)->init();
        s->loadGraphicWithParam1("img_EnemyBulletExplode.png", true, false, 0x30, 0x30);
        s->addAnimation("explode", {0, 1, 2, 3}, 15.0f, false);
        s->exists = false;
        _eBulletExplosions->add(s);
    }
    _silhouetteImage = (new FlxSprite)->init();
    _silhouetteImage->loadGraphic("img_silhouette.png");
    _silhouetteImage->setX((float)(FlxG::width() / 2) + _silhouetteImage->getWidth() * -0.5f);
    _silhouetteImage->setY((float)FlxG::height() - _silhouetteImage->getHeight());
    _silhouetteImage->exists = true;
    add(_silhouetteImage);
    _silhouette = true;
    _silhouetteTime = 1.7f;
}

// ================================================================ update / render
// - (void) update   @0x2e8c4
void PlayState::update() {
    if (!_gameOver && !_silhouette) {
        _gameTime = _gameTime + FlxG::elapsed();
        // PORT test hook: ZIGGURAT_KILL_AT=<seconds> ends a (god-mode) run at a fixed game time so
        // stats-related enhancements can be verified headlessly. Not reachable from the launcher.
        if (const char* k = zig::testEnv("ZIGGURAT_KILL_AT")) {
            if (!_gameOver && _gameTime >= (float)atof(k)) {
                BOOL g = _godMode;
                _godMode = false;
                hitPlayerWith(_player);
                _godMode = g;
            }
        }
        if (_moonRocksActive) _moonRockDelayTimer = _moonRockDelayTimer - FlxG::elapsed();
        _background->update();
        if (!_UFOappeared) {
            _UFOtimer = _UFOtimer - FlxG::elapsed();
            if (_UFOtimer <= 0.0f) {
                _UFOappeared = true;
                _UFO->exists = true;
            }
        }
        if (_moonRockDelayTimer < 0.0f) updateMoonRocks();
        updateEnemies();
        FlxState::update();
        handleCollisions();
        cleanUp();
    } else if (!_silhouette) {
        if (_gameOver) _gameOverTime = _gameOverTime + FlxG::elapsed();
    } else {
        _silhouetteTime = _silhouetteTime - FlxG::elapsed();
        if (_silhouetteTime <= 0.0f) {
            FlxG::playMusic("Ziggurat.mp3");
            FlxG::play("gameOver.wav");
            _silhouette = false;
            _silhouetteImage->exists = false;
        }
    }
    float t = _gameOverTime;
    if (1.5f < t) {
        if (!FlxG::playing("scream.wav")) {
            FlxG::stop("deathBlow.wav");
            FlxG::play("scream.wav", 0.4f);
            _gameOverTime = 0;
            return;
        }
        t = _gameOverTime;
    }
    if (2.0f < t && FlxG::playing("scream.wav")) {
        FlxG::stop("scream.wav");
        FlxG::stop("deathBlow.wav");
        updateStats();
        // - initWithScore:withManagedObjectContext:  (no death-screen image in v1.0.1)
        FlxG::setState(makeGameOverState(_numKilled, enh().showDeathSnapshot ? gDeathSnapshot : nullptr));
    }
}

// - (void) render   @0x393b4   (while the death scream plays only the red clear colour is shown)
void PlayState::render() {
    if (!_gameOver || !FlxG::playing("scream.wav")) {
        _background->render();
        FlxState::render();
        if (!_silhouette) _background->renderForeground();
    }
}

// ================================================================ spawning
int PlayState::randRangeFrom(int a, int b) { return (int)(zrand() % (uint32_t)((1 - a) + b)) + a; }

// @0x2ed14
void PlayState::addBlockingBoxWithX(float X, float Y, float w, float h) {
    FlxSprite* box = FlxSprite::spriteWithX(X, Y, std::string());
    box->setWidth(w);
    box->setHeight(h);
    box->fixed = true;
    _blockingBoxes->add(box);
}

// @0x2eda0
void PlayState::updateMoonRocks() {
    _moonRockTimer = _moonRockTimer - FlxG::elapsed();
    for (auto* m : _moonRocks->members) {
        if (!m->exists && _moonRockTimer <= 0.0f) {
            as<MoonRock>(m)->reset();
            _moonRockTimer = 1.0f;
        }
    }
}

// @0x2eed0
void PlayState::updateEnemies() {
    if (_currentEnemyLvl < 4 || (unsigned)(_currentEnemyLvl - 5) < 2) {
        _enemyLvlTimer = _enemyLvlTimer - FlxG::elapsed();
        if (_enemyLvlTimer <= 0.0f) {
            _currentEnemyLvl = _currentEnemyLvl + 1;
            _enemyLvlTimer = kEnemyLvlTime[_currentEnemyLvl];
            SDL_Log("enemy lvl: %d", _currentEnemyLvl);
        }
    }
    _blueSpawnTimer = _blueSpawnTimer - FlxG::elapsed();
    if (0 < _currentEnemyLvl) {
        _giantSpawnTimer = _giantSpawnTimer - FlxG::elapsed();
        int lvl = _currentEnemyLvl;
        if (4 < lvl) {
            _bigPurpleSpawnTimer = _bigPurpleSpawnTimer - FlxG::elapsed();
            lvl = _currentEnemyLvl;
        }
        if (0 < lvl) {
            _yellowSpawnTimer = _yellowSpawnTimer - FlxG::elapsed();
            if (1 < _currentEnemyLvl) {
                _redSpawnTimer = _redSpawnTimer - FlxG::elapsed();
                if (2 < _currentEnemyLvl) {
                    _orangeSpawnTimer = _orangeSpawnTimer - FlxG::elapsed();
                    if (4 < _currentEnemyLvl) _purpleSpawnTimer = _purpleSpawnTimer - FlxG::elapsed();
                }
            }
        }
    }
    const int L = _currentEnemyLvl;
    auto sideX = [&](float other) { return randRangeFrom(0, 1) == 0 ? (float)FlxG::width() : other; };
    if (_blueSpawnTimer <= 0.0f) {
        _blueSpawnTimer = kBlueSpawn[L];
        if (randRangeFrom(0, 100) < 0x19) {
            int move = randRangeFrom((int)kBlueMoveMin[L], (int)kBlueMoveMax[L]);
            int bullet = randRangeFrom((int)kBlueBulletMin[L], (int)kBlueBulletMax[L]);
            int fire = randRangeFrom((int)kBlueFireMin[L], (int)kBlueFireMax[L]);
            int y = randRangeFrom(100, 0x104);
            float x = sideX(-28.0f);
            addBlueAlienAtX(x, (float)y, 120.0f, (float)move, (float)bullet, (float)fire, true);
        } else {
            int jump = randRangeFrom(0x96, 200);
            int move = randRangeFrom((int)kBlueMoveMin[L], (int)kBlueMoveMax[L]);
            int bullet = randRangeFrom((int)kBlueBulletMin[L], (int)kBlueBulletMax[L]);
            int fire = randRangeFrom((int)kBlueFireMin[L], (int)kBlueFireMax[L]);
            int y = randRangeFrom(100, 0x104);
            float x = sideX(-28.0f);
            addBlueAlienAtX(x, (float)y, (float)jump, (float)move, (float)bullet, (float)fire, false);
        }
    }
    if (_giantSpawnTimer <= 0.0f) {
        _giantSpawnTimer = kGiantSpawn[L];
        float x = randRangeFrom(0, 1) == 0 ? (float)(FlxG::width() + 0x18) : -100.0f;
        addBigAlienAtX(x, 100.0f, 150.0f, 7.0f, 0.0f, 0.0f);
    }
    if (_bigPurpleSpawnTimer <= 0.0f) {
        _bigPurpleSpawnTimer = 300.0f;
        float x = randRangeFrom(0, 1) == 0 ? (float)(FlxG::width() + 0x18) : -100.0f;
        addBigPurpleAlienAtX(x, 100.0f, 150.0f, 3.0f, 0.0f, 0.0f);
    }
    if (_yellowSpawnTimer <= 0.0f) {
        _yellowSpawnTimer = kYellowSpawn[L];
        addYellowAlienAtX(sideX(-28.0f), 265.0f, 28.0f);
    }
    if (_redSpawnTimer <= 0.0f) {
        _redSpawnTimer = kRedSpawn[L];
        addRedAlienAtX(sideX(-18.0f), 200.0f, 2.0f);
    }
    if (_orangeSpawnTimer <= 0.0f) {
        _orangeSpawnTimer = kOrangeSpawn[L];
        addOrangeAlienAtX(sideX(-18.0f), 200.0f, 2.0f);
    }
    if (_purpleSpawnTimer <= 0.0f) {
        _purpleSpawnTimer = 32.0f;
        int side = randRangeFrom(0, 1);
        int ms = randRangeFrom(0xa0, 0xbe);
        float x = side == 0 ? (float)FlxG::width() : -18.0f;
        addPurpleAlienAtX(x, 200.0f, (float)ms);
    }
}

static FlxObject* firstDead(FlxGroup* g) {
    for (auto* m : g->members)
        if (!m->exists) return m;
    return nullptr;
}

// @0x35c34
void PlayState::addBlueAlienAtX(float X, float Y, float jump, float move, float bullet, float fire, BOOL floaty) {
    if (auto* a = as<Alien>(firstDead(_enemies))) {
        if (floaty) a->resetFloatyWithX(X, Y, jump, move, bullet, fire);
        else a->resetWithX(X, Y, jump, move, bullet, fire);
        return;
    }
    SDL_Log("reached blue alien cap");
}
// @0x35d64
void PlayState::addBigAlienAtX(float X, float Y, float jump, float move, float bullet, float fire) {
    if (auto* a = as<Alien>(firstDead(_bigEnemies))) { a->resetWithX(X, Y, jump, move, bullet, fire); return; }
    SDL_Log("reached big alien cap");
}
// @0x35e80
void PlayState::addBigPurpleAlienAtX(float X, float Y, float jump, float move, float bullet, float fire) {
    if (auto* a = as<Alien>(firstDead(_bigPurpleEnemies))) { a->resetWithX(X, Y, jump, move, bullet, fire); return; }
    SDL_Log("reached big purple alien cap");
}
// @0x35f9c
void PlayState::addYellowAlienAtX(float X, float Y, float ms) {
    if (auto* a = as<Alien>(firstDead(_yellowEnemies))) { a->resetWithXYMoveSpeed(X, Y, ms); return; }
    SDL_Log("reached yellow alien cap");
}
// @0x360a0
void PlayState::addRedAlienAtX(float X, float Y, float ms) {
    if (auto* a = as<Alien>(firstDead(_redEnemies))) { a->resetWithXYMoveSpeed(X, Y, ms); return; }
    SDL_Log("reached red alien cap");
}
// @0x361a4
void PlayState::addOrangeAlienAtX(float X, float Y, float ms) {
    if (auto* a = as<Alien>(firstDead(_orangeEnemies))) { a->resetWithXYMoveSpeed(X, Y, ms); return; }
    SDL_Log("reached orange alien cap");
}
// @0x362a8
void PlayState::addPurpleAlienAtX(float X, float Y, float ms) {
    if (auto* a = as<Alien>(firstDead(_purpleEnemies))) { a->resetWithXYMoveSpeed(X, Y, ms); return; }
    SDL_Log("reached purple alien cap");
}
// @0x363ac & siblings
void PlayState::addGibs(FlxGroup* g, float X, float Y, const char* color) {
    if (auto* c = as<GibCluster>(firstDead(g))) { c->resetWithXY(X, Y); return; }
    SDL_Log("reached %s gib cap", color);
}
// @0x35b20
void PlayState::addEnemyBulletAtX(float X, float Y, float vx, float vy, float speed) {
    if (auto* b = as<EnemyBullet>(firstDead(_enemyBullets))) { b->resetWithX(X, Y, vx, vy, speed); return; }
    SDL_Log("reached enemy bullet cap");
}

// Shared body of addPBulletExplosion / addEBulletExplosion (radius is ignored by the original).
void PlayState::addExplosionSprite(FlxGroup* g, float X, float Y, const char* capMsg) {
    if (auto* s = as<FlxSprite>(firstDead(g))) {
        s->setX(X);
        s->setY(Y);
        s->setX(s->x - s->getWidth() * 0.5f);
        s->setY(s->y - s->getHeight() * 0.5f);
        s->exists = true;
        s->play("explode", true);
        return;
    }
    SDL_Log("%s", capMsg);
}
// @0x35074
void PlayState::addPBulletExplosionWithX(float X, float Y, float) {
    addExplosionSprite(_pBulletExplosions, X, Y, "reached player bullet explosion effect cap");
}
// @0x35238
void PlayState::addEBulletExplosionWithX(float X, float Y, float) {
    addExplosionSprite(_eBulletExplosions, X, Y, "reached enemy bullet explosion effect cap");
}

// - (float) distanceBetweenPoint:andPoint:   @0x39388 — returns the SQUARED distance
float PlayState::distanceBetweenPoint(CGPoint a, CGPoint b) {
    float dx = b.x - a.x, dy = b.y - a.y;
    return dx * dx + dy * dy;
}

static inline bool withinBox(float cx, float cy, float X, float Y, float r) {
    int dx = (int)(cx - X);
    if (!((float)(dx < 0 ? -dx : dx) <= r)) return false;
    int dy = (int)(cy - Y);
    return (float)(dy < 0 ? -dy : dy) <= r;
}

// @0x34974
void PlayState::addExplosionWithX(float X, float Y, float r) {
    if (auto* s = as<FlxSprite>(firstDead(_explosions))) {
        s->setX(X);
        s->setY(Y);
        s->setX(s->x - s->getWidth() * 0.5f);
        s->setY(s->y - s->getHeight() * 0.5f);
        float d = r + r;
        s->scale = CGPointMake(d / s->getWidth(), d / s->getHeight());
        s->exists = true;
        s->play("explode", true);
    } else {
        SDL_Log("reached explosion cap");
    }
    float r2 = r * r;
    for (auto* m : _enemies->members) {  // only blue aliens chain-react
        auto* e = as<Alien>(m);
        if (!e->exists) continue;
        float cx = e->x + e->getWidth() * 0.5f, cy = e->y + e->getHeight() * 0.5f;
        if (!withinBox(cx, cy, X, Y, r)) continue;
        float d2 = distanceBetweenPoint(CGPointMake(cx, cy), CGPointMake(X, Y));
        if (d2 <= r2 && (e->state() == 0 || e->state() == 3 || e->state() == 4)) {
            _numKilled++;
            _blueKills++;
            incrementConsecutiveKills();
            e->hitExplosionWithRadius(r);
        }
    }
    for (auto* m : _enemyBullets->members) {
        auto* b = as<EnemyBullet>(m);
        if (!b->exists) continue;
        float cx = b->x + b->getWidth() * 0.5f, cy = b->y + b->getHeight() * 0.5f;
        if (!withinBox(cx, cy, X, Y, r)) continue;
        if (distanceBetweenPoint(CGPointMake(cx, cy), CGPointMake(X, Y)) <= r2) {
            b->exists = false;
            addEBulletExplosionWithX(b->x + b->getWidth() * 0.5f, b->y + b->getHeight() * 0.5f, 1.0f);
        }
    }
    FlxG::play("explode.wav", 0.35f);
    FlxG::quake()->start(0.02f, 0.15f);
}

// @0x353fc
void PlayState::addLightningAtX(float X, float Y, float w) {
    float half = w * 0.5f;
    float lo = X - half, hi = half + X;
    auto zap = [&](FlxGroup* g, unsigned int& kills, bool needVulnerable) {
        for (auto* m : g->members) {
            auto* e = as<Alien>(m);
            if (!e->exists) continue;
            if (needVulnerable && e->vulnerable() != 1) continue;
            float cx = e->x + e->getWidth() * 0.5f;
            if (lo <= cx && cx <= hi && Y < e->y) {
                e->headshotByBulletWithLvl(3);
                _numKilled++;
                kills++;
            }
        }
    };
    zap(_enemies, _blueKills, false);
    zap(_yellowEnemies, _yellowKills, false);
    zap(_redEnemies, _redKills, false);
    zap(_purpleEnemies, _purpleKills, false);
    zap(_orangeEnemies, _orangeKills, true);
}

// @0x34874
void PlayState::hitPlayerWith(FlxObject* o) {
    if (_godMode) return;
    // ENHANCEMENT showDeathSnapshot: keep the last rendered frame (the moment of death) for GameOverState.
    if (enh().showDeathSnapshot && !_gameOver) gDeathSnapshot = plat::snapshot();
    _player->die();
    _killer = o;
    _gameOver = true;
    setBgColor(enh().reduceFlashing ? 0xff4c0000 : 0xffff0000);  // ENHANCEMENT reduceFlashing: 30 % red
    FlxG::pauseMusic();
    FlxG::quake()->start(0.025f, 0.1f);
    FlxG::play("deathBlow.wav", 1.0f);
    _UFO->stopSound();
}

// @0x3685c
void PlayState::killAllEnemies() {
    for (FlxGroup* g : {_enemies, _yellowEnemies, _redEnemies, _orangeEnemies, _purpleEnemies, _bigEnemies, _bigPurpleEnemies})
        for (auto* m : g->members) m->exists = false;
    for (auto* m : _enemyBullets->members) m->exists = false;
    _blueSpawnTimer += 8.0f;
    _giantSpawnTimer += 8.0f;
    _bigPurpleSpawnTimer += 8.0f;
    _yellowSpawnTimer += 8.0f;
    _redSpawnTimer += 8.0f;
    _orangeSpawnTimer += 8.0f;
    _purpleSpawnTimer += 8.0f;
    _currentEnemyLvl = 5;
    _shotsUntilUniverseEnd = _player->shotsFired();
    _moonRocksActive = true;
}

// @0x392f8
void PlayState::incrementConsecutiveKills() {
    _consecutiveKills++;
    if (_highestConsecutiveKills < _consecutiveKills) _highestConsecutiveKills = _consecutiveKills;
}
// @0x39328
void PlayState::clearConsecutiveKills() {
    _missedShots++;
    _consecutiveKills = 0;
    SDL_Log("MISSED consecutive kills: %u, misses: %u", _consecutiveKills, _missedShots);
}

// ================================================================ collisions   @0x2f808
static inline CGPoint centerOf(FlxObject* o) { return CGPointMake(o->x + o->getWidth() * 0.5f, o->y + o->getHeight() * 0.5f); }
static inline int absTrunc(float v) { int i = (int)v; return i < 0 ? -i : i; }

void PlayState::handleCollisions() {
    Player* P = _player;
    // 1. Toppling (body-shot) aliens stun the blue aliens they hit.
    for (auto* m : _enemies->members) {
        auto* e = as<Alien>(m);
        for (Alien* t : _topplingEnemies) {
            if (e->overlaps(t) && e != t) {
                e->stunWithVelocity(t->velocity.x, t->velocity.y);
                if (!FlxG::playing("enemyTopple.wav")) FlxG::play("enemyTopple.wav", 1.0f);
            }
        }
    }
    FlxU::alternateCollideWithParam1(_enemies, _enemies);
    FlxU::alternateCollideWithParam1(_gibClustersBlue, _blockingBoxes);
    FlxU::alternateCollideWithParam1(_gibClustersYellow, _blockingBoxes);
    FlxU::alternateCollideWithParam1(_gibClustersRed, _blockingBoxes);
    FlxU::alternateCollideWithParam1(_gibClustersOrange, _blockingBoxes);
    FlxU::alternateCollideWithParam1(_gibClustersPurple, _blockingBoxes);

    auto pexp = [&](FlxObject* b) { CGPoint c = centerOf(b); addPBulletExplosionWithX(c.x, c.y, 1.0f); };
    auto eexp = [&](FlxObject* b) { CGPoint c = centerOf(b); addEBulletExplosionWithX(c.x, c.y, 1.0f); };
    auto kill = [&](unsigned int& counter) {
        _numKilled++;
        counter++;
        incrementConsecutiveKills();
    };
    auto rockHot = [](MoonRock* r) { return r->exists && r->_toppling == 1 && 0x96 < absTrunc(r->velocity.x); };

    // 2. Blue aliens.
    for (auto* m : _enemies->members) {
        auto* e = as<Alien>(m);
        e->onAlien = false;
        if (e->onFloor) {  // standing on another alien (set by alternateCollide above)
            e->onAlien = true;
            e->onFloor = false;
        }
        if (e->state() == 1 || !e->exists) continue;
        FlxU::collideObject(e, _blockingBoxes);
        for (auto* bm : _playerBullets->members) {
            auto* b = as<PlayerBullet>(bm);
            bool headPath = e->headSprite()->overlaps(b);
            if (!headPath && e->overlaps(b) && e->state() != 0) headPath = true;
            if (headPath) {
                e->headshotByBulletWithLvl(b->level());
                pexp(b);
                b->active = false;
                _headShots++;
                if (e->state() != 2) kill(_blueKills);
            } else if (e->overlaps(b)) {
                if (b->level() < 4) {
                    e->bodyshotWithVelocity(b->velocity.x, b->velocity.y);
                    _topplingEnemies.push_back(e);
                    FlxG::play("enemyHit.wav", 0.7f);
                    FlxG::quake()->start(0.02f, 0.1f);
                } else {
                    e->headshotByBulletWithLvl(b->level());
                }
                pexp(b);
                b->active = false;
                kill(_blueKills);
            }
        }
        if (e->state() == 0) {
            if (e->overlaps(P->hitbox())) {
                hitPlayerWith(e);
                _killerType = 0;
            }
            if (e->overlaps(P->chargingShot()) && P->chargingShot()->visible) {
                CGPoint v = P->shotVector();
                e->bodyshotWithVelocity(v.x, v.y);
                _topplingEnemies.push_back(e);
                FlxG::play("enemyHit.wav", 0.7f);
                P->cancelChargeForced(true);
                kill(_blueKills);
            }
            for (auto* rm : _moonRocks->members) {
                auto* r = as<MoonRock>(rm);
                if (rockHot(r) && e->overlaps(r)) {
                    CGPoint ce = centerOf(e), cr = centerOf(r);
                    float dx = ce.x - cr.x, dy = ce.y - cr.y;
                    e->hitWithMoonRockWithVelocity(dx + dx, dy + dy);
                    FlxG::play("enemyTopple.wav", 1.0f);
                }
            }
        }
        if (e->state() == 3 && (float)FlxG::height() < e->y && e->exists) {
            e->exists = false;
            kill(_blueKills);
        }
    }

    // 3/4. Big aliens (blue giants and the big purple one).
    auto bigPass = [&](FlxGroup* g, unsigned int& kills, int killerType, bool scream) {
        for (auto* m : g->members) {
            auto* e = as<BigAlien>(m);
            if (e->state() == 1 || !e->exists) continue;
            e->collideBottom = false;
            e->collideRight = false;
            e->collideLeft = false;
            FlxU::collideObject(e, _enemies);
            e->onFloor = false;
            e->collideBottom = true;
            e->collideRight = true;
            e->collideLeft = true;
            FlxU::collideObject(e, _blockingBoxes);
            for (auto* bm : _playerBullets->members) {
                auto* b = as<PlayerBullet>(bm);
                if (!(e->headSprite()->overlaps(b) || e->hitbox()->overlaps(b))) continue;
                pexp(b);
                b->active = false;
                if (1 < b->level()) {
                    e->headshotByBulletWithLvl(b->level());
                    if (e->state() == 1) {
                        kill(kills);
                        if (scream) FlxG::play("scream.wav", 0.6f);
                    }
                    if (e->headSprite()->overlaps(b)) _headShots++;
                }
            }
            if (e->state() == 0) {
                if (e->hitbox()->overlaps(P->hitbox())) {
                    hitPlayerWith(e);
                    _killerType = killerType;
                }
                for (auto* rm : _moonRocks->members) {
                    auto* r = as<MoonRock>(rm);
                    if (!(rockHot(r) && e->overlaps(r))) continue;
                    float rv = r->velocity.x, ev = e->velocity.x;
                    if ((0.0f < rv && ev < 0.0f) || (rv < 0.0f && 0.0f < ev)) {  // head-on only
                        r->velocity = CGPointMake(r->velocity.x * -0.3f, r->velocity.y);
                        e->headshotByBulletWithLvl(3);
                        FlxG::play("enemyTopple.wav", 1.0f);
                    }
                }
            }
        }
    };
    bigPass(_bigEnemies, _giantKills, 4, false);
    bigPass(_bigPurpleEnemies, _bigPurpleKills, 5, true);

    // 5-8. Small coloured aliens.
    auto smallPass = [&](FlxGroup* g, unsigned int& kills, int killerType, bool jumper, bool needVulnerable,
                         unsigned int* topplingCounter, bool rocks) {
        for (auto* m : g->members) {
            auto* e = as<Alien>(m);
            if (e->state() == 1 || !e->exists) continue;
            FlxU::collideObject(e, _blockingBoxes);
            if (jumper)
                for (auto* box : _blockingBoxes->members)
                    if (e->headSprite()->overlaps(box)) e->jump();
            for (auto* bm : _playerBullets->members) {
                auto* b = as<PlayerBullet>(bm);
                if (!(e->headSprite()->overlaps(b) || e->overlaps(b))) continue;
                if (!needVulnerable) e->headshotByBulletWithLvl(b->level());
                pexp(b);
                b->active = false;
                if (needVulnerable) {
                    if (e->vulnerable() != 1) continue;
                    e->headshotByBulletWithLvl(b->level());
                }
                kill(kills);
                if (e->headSprite()->overlaps(b)) _headShots++;
            }
            if (e->state() != 0) continue;
            if (e->overlaps(P->hitbox())) {
                hitPlayerWith(e);
                _killerType = killerType;
            }
            if (e->overlaps(P->chargingShot()) && P->chargingShot()->visible) {
                e->headshotByBulletWithLvl(1);
                P->cancelChargeForced(true);
                kill(kills);
            }
            if (topplingCounter) {
                for (Alien* t : _topplingEnemies) {
                    if (e->overlaps(t) && e != t) {
                        e->headshotByBulletWithLvl(2);
                        kill(*topplingCounter);
                    }
                }
            }
            if (rocks) {
                for (auto* rm : _moonRocks->members) {
                    auto* r = as<MoonRock>(rm);
                    if (rockHot(r) && e->overlaps(r) && (!needVulnerable || e->vulnerable() == 1)) {
                        e->headshotByBulletWithLvl(1);
                        FlxG::play("enemyTopple.wav", 1.0f);
                    }
                }
            }
        }
    };
    // ORIGINAL BUG kept: yellow aliens killed by a toppling alien are counted as red kills.
    // ENHANCEMENT fixYellowAsRed counts them as yellow kills.
    smallPass(_yellowEnemies, _yellowKills, 1, true, false, enh().fixYellowAsRed ? &_yellowKills : &_redKills, true);
    smallPass(_redEnemies, _redKills, 2, false, false, &_redKills, true);
    smallPass(_orangeEnemies, _orangeKills, 3, false, true, nullptr, true);
    smallPass(_purpleEnemies, _purpleKills, 6, false, false, &_purpleKills, false);

    // 9. Player bullets vs enemy bullets, the ziggurat and the UFO.
    for (auto* bm : _playerBullets->members) {
        auto* b = as<PlayerBullet>(bm);
        for (auto* em : _enemyBullets->members) {
            auto* eb = as<EnemyBullet>(em);
            if (!eb->exists || !b->overlaps(eb->hitbox())) continue;
            if (b->level() < 3) {
                pexp(b);
                b->active = false;
            }
            eexp(eb);
            eb->exists = false;
            FlxG::play("enemyBulletDestroyed.wav", 1.0f);
        }
        for (auto* box : _blockingBoxes->members) {
            if (!b->overlaps(box)) continue;
            if (0.0f < b->velocity.y) {
                if (b->bounce() == 1) {
                    b->velocity = CGPointMake(b->velocity.x, b->velocity.y * -0.5f);
                } else {
                    // sic: the explosion is centred using the charging shot's size
                    addPBulletExplosionWithX(b->x + P->chargingShot()->getWidth() * 0.5f,
                                             b->y + P->chargingShot()->getHeight() * 0.5f, 1.0f);
                    b->active = false;
                    clearConsecutiveKills();
                }
            }
        }
        if (_UFO->exists && b->overlaps(_UFO)) {
            _UFO->charge();
            b->active = false;
            _ufoHits++;
        }
    }

    // 10. Enemy bullets vs the ziggurat, the player and the charging shot.
    for (auto* em : _enemyBullets->members) {
        auto* eb = as<EnemyBullet>(em);
        if (!eb->exists) continue;
        for (auto* box : _blockingBoxes->members) {
            if (eb->hitbox()->overlaps(box)) {
                eb->exists = false;
                eexp(eb);
            }
        }
        if (eb->hitbox()->overlaps(P->hitbox())) {
            hitPlayerWith(eb);
            _killerType = 7;
        }
        if (eb->hitbox()->overlaps(P->chargingShot()) && P->chargingShot()->visible) {
            eb->exists = false;
            P->cancelCharge();
            eexp(eb);
        }
    }

    // 11. Moon rocks bounce on the ziggurat and get knocked by bullets.
    for (auto* rm : _moonRocks->members) {
        auto* r = as<MoonRock>(rm);
        if (!r->exists) continue;
        for (auto* box : _blockingBoxes->members)
            if (r->overlaps(box)) r->bounce();
        for (auto* bm : _playerBullets->members) {
            auto* b = as<PlayerBullet>(bm);
            if (!b->exists || !r->overlaps(b)) continue;
            pexp(b);
            b->active = false;
            float k = b->level() < 4 ? 0.1f : 0.8f;
            r->velocity = CGPointMake(r->velocity.x + b->velocity.x * k, r->velocity.y + b->velocity.y * k);
            r->_toppling = true;
        }
    }
}

// @0x33e0c
void PlayState::cleanUp() {
    for (FlxGroup* g : {_explosions, _pBulletExplosions, _eBulletExplosions})
        for (auto* m : g->members)
            if (as<FlxSprite>(m)->finished) m->exists = false;
    auto offscreen = [](FlxSprite* s) {
        return !(0.0f - s->frameWidth <= s->x && s->x <= (float)FlxG::width() + s->frameWidth && s->y <= (float)FlxG::height());
    };
    for (auto* m : _enemyBullets->members)
        if (m->exists && offscreen(as<FlxSprite>(m))) m->exists = false;
    for (int i = (int)_playerBullets->members.size() - 1; i >= 0; i--) {
        FlxObject* b = _playerBullets->members[i];
        bool remove = b->x < -50.0f || (float)FlxG::width() < b->x || (float)FlxG::height() < b->y || !b->active;
        if (remove) {
            if (b->active) clearConsecutiveKills();
            _playerBullets->remove(b);
            delete b;
        }
    }
    for (int i = (int)_topplingEnemies.size() - 1; i >= 0; i--) {
        if (i >= (int)_topplingEnemies.size()) continue;
        Alien* a = _topplingEnemies[i];
        if (!a->exists) _topplingEnemies.erase(std::remove(_topplingEnemies.begin(), _topplingEnemies.end(), a), _topplingEnemies.end());
    }
    for (auto* m : _moonRocks->members)
        if (m->exists && offscreen(as<FlxSprite>(m))) m->exists = false;
}

// ================================================================ stats   @0x36f14
void PlayState::updateStats() {
    if (enh().practiceActive()) {  // ENHANCEMENT (practice): never record practice runs
        SDL_Log("stats: practice mode active - game not recorded");
        return;
    }
    Statistics& S = Statistics::shared();
    auto addI = [&](const char* k, double v) { S.set(k, (double)(S.getInt(k) + (int)v)); };
    const unsigned shots = _player->shotsFired();
    addI("kills", _numKilled);
    addI("missedShots", _missedShots);
    addI("shots", shots);
    int games = S.getInt("gamesPlayed");
    S.set("averageKills", (S.getFloat("averageKills") * (float)games + (float)_numKilled) / (float)(games + 1));
    S.set("averageSeconds", (S.getFloat("averageSeconds") * (float)games + _gameTime) / (float)(games + 1));
    S.set("averageShots", (S.getFloat("averageShots") * (float)games + (float)shots) / (float)(games + 1));
    S.set("gamesPlayed", games + 1);
    if (4 < _currentEnemyLvl) addI("moonExplosions", 1);
    S.set("secondsPlayed", S.getFloat("secondsPlayed") + _gameTime);
    addI("blueKills", _blueKills);
    addI("yellowKills", _yellowKills);
    addI("redKills", _redKills);
    addI("orangeKills", _orangeKills);
    addI("purpleKills", _purpleKills);
    addI("giantKills", _giantKills);
    addI("bigPurpleKills", _bigPurpleKills);
    if (!enh().fixPurpleDoubleCount) addI("purpleKills", _purpleKills);  // sic: added twice in the original (ENHANCEMENT fixPurpleDoubleCount skips it)
    addI("headShots", _headShots);
    addI("ufoHits", _ufoHits);
    if ((unsigned)S.getInt("bestKills") < _numKilled) {
        S.set("bestKills", _numKilled);
        S.set("bestSeconds", _gameTime);
        S.set("bestShots", shots);
        S.set("bestMissedShots", _missedShots);
    }
    if ((unsigned)S.getInt("bestUfoHits") < _ufoHits) S.set("bestUfoHits", _ufoHits);
    static const char* const kDeaths[8] = {"blueDeaths", "yellowDeaths", "redDeaths", "orangeDeaths",
                                           "giantDeaths", "bigPurpleDeaths", "purpleDeaths", "bulletDeaths"};
    if (0 <= _killerType && _killerType < 8) addI(kDeaths[_killerType], 1);
    S.set("lastKills", _numKilled);
    S.set("lastMissedShots", _missedShots);
    S.set("lastSecondsPlayed", (int)_gameTime);
    S.set("lastShots", shots);
    S.set("lastUfoHits", _ufoHits);
    S.set("lastHeadshots", _headShots);
    S.set("lastBlueKills", _blueKills);
    S.set("lastYellowKills", _yellowKills);
    S.set("lastRedKills", _redKills);
    S.set("lastOrangeKills", _orangeKills);
    S.set("lastGiantKills", _giantKills);
    S.set("lastPurpleKills", _purpleKills);
    S.set("lastBigPurpleKills", _bigPurpleKills);
    // PORT: OpenFeint achievements (1488232..1488492) and leaderboards (1065777, 10665x7) dropped — service is gone.
    S.save();
    SDL_Log("highest consecutive kills: %u", _highestConsecutiveKills);
}
