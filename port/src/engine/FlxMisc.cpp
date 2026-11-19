//
//  FlxMisc.cpp — FlxState.m, FlxEmitter.m, FlxButton.m, FlxTouches.m, FlxSave.m
//
//  Copyright Semi Secret Software 2009-2010. All rights reserved.
//  C++ port: MIT license (see ENGINE_LICENSE.txt).
//
#include "Flixel.h"

// ============================== FlxState ==============================
FlxState* FlxState::init() {
    gFlxZone = &zone;  // PORT: everything allocated from now on belongs to this state (see FlxZone)
    defaultGroup = (new FlxGroup)->init();
    bgColor = 0x000000;
    return this;
}
// - (void) setBgColor:(unsigned int)BGColor;
void FlxState::setBgColor(unsigned int BGColor) {
    bgColor = BGColor;
    red = ((bgColor >> 16) & 0xff) / 255.0;
    green = ((bgColor >> 8) & 0xff) / 255.0;
    blue = ((bgColor >> 0) & 0xff) / 255.0;
    alpha = ((bgColor >> 24) & 0xff) / 255.0;
}
// - (void) preProcess;   glClearColor(red, green, blue, alpha)
void FlxState::preProcess() { plat::setClearColor(red, green, blue, alpha); }

// ============================== FlxEmitter ==============================
// - (id) initWithX:(float)X y:(float)Y;
FlxEmitter* FlxEmitter::initWithEmitterX(float X, float Y) {
    FlxGroup::initWithX(X, Y, 0, 0);
    x = X;
    y = Y;
    width = 0;
    height = 0;
    minParticleSpeed = CGPointMake(-100, -100);
    maxParticleSpeed = CGPointMake(100, 100);
    minRotation = -360;
    maxRotation = 360;
    gravity = 400;
    particleDrag = CGPointZero;
    delay = 0.1;
    quantity = 0;
    _counter = 0;
    _explode = true;
    exists = false;
    on = false;
    return this;
}

// - (FlxEmitter *) createSprites:quantity:bakedRotations:multiple:collide:modelScale:
FlxEmitter* FlxEmitter::createSprites(const std::string& Graphics, unsigned int Quantity, unsigned int BakedRotations, BOOL Multiple,
                                      float Collide, float ModelScale) {
    members.clear();
    unsigned int r;
    FlxSprite* s;
    unsigned int tf = 1;
    float sw, sh;
    if (Multiple) {
        s = (new FlxManagedSprite)->initWithX(0, 0, Graphics, ModelScale);
        tf = s->getWidth() / s->getHeight();
    }
    for (unsigned int i = 0; i < Quantity; i++) {
        s = (new FlxSprite)->init();
        if (Multiple) {
            r = FlxU::random() * tf;
            if (BakedRotations > 0)
                s->loadRotatedGraphic(Graphics, BakedRotations, r, false, false, ModelScale);
            else {
                s->loadGraphicAnimatedModelScale(Graphics, true, ModelScale);
                s->setFrame(r);
            }
        } else {
            if (BakedRotations > 0)
                s->loadRotatedGraphic(Graphics, BakedRotations, -1, false, false, ModelScale);
            else
                s->loadGraphicModelScale(Graphics, ModelScale);
        }
        if (Collide > 0) {
            sw = s->getWidth();
            sh = s->getHeight();
            s->setWidth(s->getWidth() * Collide);
            s->setHeight(s->getHeight() * Collide);
            s->offset = CGPointMake((sw - s->getWidth()) / 2, (sh - s->getHeight()) / 2);
            s->solid = true;
        } else {
            s->solid = false;
        }
        s->exists = false;
        s->scrollFactor = scrollFactor;
        add(s);
    }
    return this;
}

// - (void) updateEmitter;
void FlxEmitter::updateEmitter() {
    if (_explode) {
        unsigned int i, l;
        _timer += FlxG::elapsed();
        if ((delay > 0) && (_timer > delay)) {
            kill();
            return;
        }
        if (on) {
            on = false;
            l = members.size();
            if (quantity > 0) l = quantity;
            l += _particle;
            for (i = _particle; i < l; i++) emitParticle();
        }
        return;
    }
    if (!on) return;
    _timer += FlxG::elapsed();
    while ((_timer > delay) && ((quantity <= 0) || (_counter < quantity))) {
        _timer -= delay;
        emitParticle();
    }
}

// - (void) updateMembers;
void FlxEmitter::updateMembers() {
    size_t l = members.size();
    for (size_t i = 0; i < l; i++) {
        FlxObject* o = members[i];
        if ((o != nullptr) && o->exists && o->active) o->update();
    }
}

void FlxEmitter::update() {
    FlxGroup::update();
    updateEmitter();
}

// - (void) startWithParam1:(BOOL)Explode param2:(float)Delay param3:(unsigned int)Quantity;
void FlxEmitter::start(BOOL Explode, float Delay, unsigned int Quantity) {
    if (members.size() <= 0) {
        FlxG::log("WARNING: there are no sprites loaded in your emitter.\nAdd some to FlxEmitter.members or use FlxEmitter.createSprites().");
        return;
    }
    _explode = Explode;
    if (!_explode) _counter = 0;
    if (!exists) _particle = 0;
    exists = true;
    visible = true;
    active = true;
    dead = false;
    on = true;
    _timer = 0;
    if (quantity == 0) quantity = Quantity;
    if (Delay != 0) delay = Delay;
    if (delay < 0) delay = -delay;
    for (auto* s : members) s->visible = false;
}

// - (void) emitParticle;
void FlxEmitter::emitParticle() {
    _counter++;
    FlxObject* s = members[_particle];
    s->exists = true;
    s->active = true;
    s->dead = false;
    s->setX(x - ((int)(s->getWidth()) >> 1) + FlxU::random() * width);
    s->setY(y - ((int)(s->getHeight()) >> 1) + FlxU::random() * height);
    s->visible = true;
    CGPoint s_velocity;
    s_velocity.x = minParticleSpeed.x;
    if (minParticleSpeed.x != maxParticleSpeed.x) s_velocity.x += FlxU::random() * (maxParticleSpeed.x - minParticleSpeed.x);
    s_velocity.y = minParticleSpeed.y;
    if (minParticleSpeed.y != maxParticleSpeed.y) s_velocity.y += FlxU::random() * (maxParticleSpeed.y - minParticleSpeed.y);
    s->velocity = s_velocity;
    s->acceleration = CGPointMake(s->acceleration.x, gravity);
    s->angularVelocity = minRotation;
    if (minRotation != maxRotation) s->angularVelocity += FlxU::random() * (maxRotation - minRotation);
    if (s->angularVelocity != 0) s->angle = FlxU::random() * 360 - 180;
    s->drag = particleDrag;
    _particle++;
    if (_particle >= members.size()) _particle = 0;
    s->onEmit();
}

// - (void) stop:(float)Delay;
void FlxEmitter::stop(float Delay) {
    _explode = true;
    delay = Delay;
    if (delay < 0) delay = -Delay;
    on = false;
}
// - (void) at:(FlxObject *)Object;
void FlxEmitter::at(FlxObject* Object) {
    x = Object->x + Object->origin.x;
    y = Object->y + Object->origin.y;
}
void FlxEmitter::kill() {
    FlxGroup::kill();
    on = false;
}

// ============================== FlxButton ==============================
// - (id) initWithX:(int)X y:(int)Y callback:(FlashFunction *)Callback;
FlxButton* FlxButton::initWithX(int X, int Y, FlashFunction* Callback) {
    x = X;
    y = Y;
    FlxGroup::init();
    x = X;
    y = Y;
    width = 100;
    height = 20;
    _off = (new FlxSprite)->init()->createGraphicWithParam1(width, height, 0xff7f7f7f);
    _off->solid = false;
    addWithParam1(_off, true);
    _on = (new FlxSprite)->init()->createGraphicWithParam1(width, height, 0xffffffff);
    _on->solid = false;
    addWithParam1(_on, true);
    _offT = nullptr;
    _onT = nullptr;
    _callback = Callback;
    _onToggle = false;
    _pressed = false;
    _initialized = false;
    _sf = nullptr;
    return this;
}

// - (FlxButton *) loadGraphicWithParam1:(FlxSprite *)Image param2:(FlxSprite *)ImageHighlight;
FlxButton* FlxButton::loadGraphicWithParam1(FlxSprite* Image, FlxSprite* ImageHighlight) {
    _off = (FlxSprite*)replaceWithParam1(_off, Image);
    if (ImageHighlight == nullptr) {
        if (_on != _off) remove(_on);
        _on = _off;
    } else {
        _on = (FlxSprite*)replaceWithParam1(_on, ImageHighlight);
    }
    _on->solid = _off->solid = false;
    _off->scrollFactor = scrollFactor;
    _on->scrollFactor = scrollFactor;
    width = _off->getWidth();
    height = _off->getHeight();
    refreshHulls();
    return this;
}

// - (FlxButton *) loadTextWithParam1:(FlxText *)Text param2:(FlxText *)TextHighlight;
FlxButton* FlxButton::loadTextWithParam1(FlxText* Text, FlxText* TextHighlight) {
    if (Text) Text->setAlignment("center");
    if (TextHighlight) TextHighlight->setAlignment("center");
    if (Text != nullptr) {
        if (_offT == nullptr) {
            _offT = Text;
            add(_offT);
        } else {
            _offT = (FlxText*)replaceWithParam1(_offT, Text);
        }
    }
    if (TextHighlight == nullptr) {
        _onT = _offT;
    } else {
        if (_onT == nullptr) {
            _onT = TextHighlight;
            add(_onT);
        } else {
            _onT = (FlxText*)replaceWithParam1(_onT, TextHighlight);
        }
    }
    if (_offT) _offT->scrollFactor = scrollFactor;
    if (_onT) _onT->scrollFactor = scrollFactor;
    return this;
}

// pending FlashFunction executions ([_callback performSelector:@selector(execute) afterDelay:0.0])
std::vector<FlashFunction*>& flxDeferredCalls() {
    static std::vector<FlashFunction*> v;
    return v;
}

// - (void) update;
void FlxButton::update() {
    if (!_initialized) _initialized = true;
    FlxGroup::update();
    visibility(false);
    if (FlxG::touches()->touching) {
        CGPoint p = FlxG::touches()->screenTouchPoint;
        CGPoint selfP = getScreenXY();
        _pressed = CGRectContainsPoint(CGRectMake(selfP.x, selfP.y, width, height), p);
        visibility(_pressed);
    }
    if (FlxG::touches()->touchesEnded) {
        CGPoint lastP = FlxG::touches()->lastScreenTouchPoint;
        if (!(!exists || !visible || !active || _callback == nullptr)) {
            CGPoint selfP = getScreenXY();
            if (CGRectContainsPoint(CGRectMake(selfP.x, selfP.y, width, height), lastP)) flxDeferredCalls().push_back(_callback);
        }
    }
    if (_onToggle) visibility(_off->visible);
}

// - (void) visibility:(BOOL)On;
void FlxButton::visibility(BOOL On) {
    if (On) {
        _off->visible = false;
        if (_offT != nullptr) _offT->visible = false;
        _on->visible = true;
        if (_onT != nullptr) _onT->visible = true;
    } else {
        _on->visible = false;
        if (_onT != nullptr) _onT->visible = false;
        _off->visible = true;
        if (_offT != nullptr) _offT->visible = true;
    }
}

// ============================== FlxTouches ==============================
// - (void) processTouches:(NSSet *)newTouches
void FlxTouches::processTouches(int newCount, CGPoint location) {
    BOOL getScreenTouchBeganPoint = false;
    if (touchCount == 0 && newCount > 0) {
        nextTouchesBegan = true;
        getScreenTouchBeganPoint = true;
    }
    if (newCount == 0 && touchCount > 0) nextTouchesEnded = true;
    touchCount = newCount;
    anyTouchLocation = location;
    nextLastScreenTouchPoint = nextScreenTouchPoint;
    nextScreenTouchPoint = internalScreenTouchPoint();
    if (getScreenTouchBeganPoint) nextScreenTouchBeganPoint = nextScreenTouchPoint;
    newData = true;
}

// - (void) update
void FlxTouches::update() {
    touchesBegan = false;
    touchesEnded = false;
    if (newData) {
        touchesBegan = nextTouchesBegan;
        touchesEnded = nextTouchesEnded;
        nextTouchesBegan = false;
        nextTouchesEnded = false;
        touching = touchCount > 0;
        lastScreenTouchPoint = nextLastScreenTouchPoint;
        screenTouchPoint = nextScreenTouchPoint;
        screenTouchBeganPoint = nextScreenTouchBeganPoint;
    }
    newData = false;
}

CGPoint FlxTouches::touchPoint() {
    CGPoint p = screenTouchPoint;
    p.x -= FlxG::scroll()->x;
    p.y -= FlxG::scroll()->y;
    return p;
}
CGPoint FlxTouches::lastTouchPoint() {
    CGPoint p = lastScreenTouchPoint;
    p.x -= FlxG::scroll()->x;
    p.y -= FlxG::scroll()->y;
    return p;
}
CGPoint FlxTouches::touchBeganPoint() {
    CGPoint p = screenTouchBeganPoint;
    p.x -= FlxG::scroll()->x;
    p.y -= FlxG::scroll()->y;
    return p;
}

// - (CGPoint) internalScreenTouchPoint
// PORT: anyTouchLocation is already in landscape UIKit points (the port has no rotated GL view), so
// the UIDeviceOrientationLandscapeLeft branch reduces to a plain division by the zoom.
CGPoint FlxTouches::internalScreenTouchPoint() {
    CGPoint p = CGPointZero;
    if (touchCount == 0) return p;
    p = anyTouchLocation;
    FlxGame* game = FlxG::game();
    float z = game->zoom();
    if (FlxG::retinaDisplay()) z = z / 2;
    p.x = p.x / z;
    p.y = p.y / z;
    if (game->textureBufferZoom) {
        p.x /= 2;
        p.y /= 2;
    }
    return p;
}

// ============================== FlxSave ==============================
// PORT: NSUserDefaults dictionary under `name` -> flat "name/field" keys in UserDefaults.
BOOL FlxSave::bind(const std::string& Name) {
    name = Name;
    bound = true;
    return true;
}
BOOL FlxSave::writeWithParam1(const std::string& FieldName, const std::string& FieldValue, unsigned int MinFileSize) {
    if (!bound) {
        FlxG::log("WARNING: You must call FlxSave.bind()\nbefore calling FlxSave.write().");
        return false;
    }
    UserDefaults::standard().set(name + "/" + FieldName, FieldValue);
    return forceSave(MinFileSize);
}
std::string FlxSave::read(const std::string& FieldName) {
    if (!bound) {
        FlxG::log("WARNING: You must call FlxSave.bind()\nbefore calling FlxSave.read().");
        return std::string();
    }
    return UserDefaults::standard().get(name + "/" + FieldName);
}
BOOL FlxSave::forceSave(unsigned int) {
    if (!bound) {
        FlxG::log("WARNING: You must call FlxSave.bind()\nbefore calling FlxSave.forceSave().");
        return false;
    }
    return UserDefaults::standard().synchronize();
}
BOOL FlxSave::erase(unsigned int MinFileSize) {
    if (!bound) {
        FlxG::log("WARNING: You must call FlxSave.bind()\nbefore calling FlxSave.erase().");
        return false;
    }
    for (auto& k : UserDefaults::standard().keysWithPrefix(name + "/")) UserDefaults::standard().remove(k);
    return forceSave(MinFileSize);
}
