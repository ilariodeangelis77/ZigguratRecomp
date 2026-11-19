//
//  FlxObject.cpp — FlxObject.m + FlxGroup.m
//
//  Copyright Semi Secret Software 2009-2010. All rights reserved.
//  C++ port: MIT license (see ENGINE_LICENSE.txt).
//
#include "Flixel.h"

// ============================== zones (PORT: memory ownership, see FlxZone in Flixel.h) ==========
FlxZone* gFlxZone = nullptr;
static void* gLastFlxAlloc = nullptr;  // set by FlxPoint::operator new, consumed by the constructor

void* FlxPoint::operator new(size_t size) {
    void* p = ::operator new(size);
    gLastFlxAlloc = p;
    return p;
}
void FlxPoint::operator delete(void* p) { ::operator delete(p); }

FlxPoint::FlxPoint() {
    // Only heap objects created with `new` register (members/stack objects have other addresses).
    if (this == gLastFlxAlloc) {
        gLastFlxAlloc = nullptr;
        if (gFlxZone) {
            zone_ = gFlxZone;
            zoneIndex_ = zone_->objects.size();
            zone_->objects.push_back(this);
        }
    }
}

void FlxPoint::zoneDetach() {
    if (!zone_) return;
    auto& v = zone_->objects;
    FlxPoint* last = v.back();
    v[zoneIndex_] = last;
    last->zoneIndex_ = zoneIndex_;
    v.pop_back();
    zone_ = nullptr;
}

FlxPoint::~FlxPoint() { zoneDetach(); }

void FlxZone::freeAll() {
    std::vector<FlxPoint*> objs;
    objs.swap(objects);
    for (FlxPoint* o : objs) o->zone_ = nullptr;  // destructors must not touch the vector being freed
    for (FlxPoint* o : objs) delete o;
    for (const std::string& k : uniqueTextures) FlxG::removeTexture(k);
    uniqueTextures.clear();
}

// ============================== FlxObject ==============================

// - (id) initWithX:(float)X y:(float)Y width:(float)Width height:(float)Height;
FlxObject* FlxObject::initWithX(float X, float Y, float Width, float Height) {
    initRect(X, Y, Width, Height);
    enableBlend = true;
    exists = true;
    active = true;
    visible = true;
    solid = true;
    fixed = false;
    moves = true;
    collideLeft = true;
    collideRight = true;
    collideTop = true;
    collideBottom = true;
    origin = CGPointZero;
    velocity = CGPointZero;
    acceleration = CGPointZero;
    drag = CGPointZero;
    maxVelocity = CGPointMake(10000, 10000);
    angle = 0;
    angularVelocity = 0;
    angularAcceleration = 0;
    angularDrag = 0;
    maxAngular = 10000;
    thrust = 0;
    scrollFactor = CGPointMake(1, 1);
    _flicker = false;
    _flickerTimer = -1;
    health = 1;
    dead = false;
    colHullX = CGRectZero;
    colHullY = CGRectZero;
    colVector = CGPointZero;
    colOffsets.assign(1, CGPointZero);
    _group = false;
    return this;
}

// - (void) refreshHulls;
void FlxObject::refreshHulls() {
    colHullX.origin.x = x;
    colHullX.origin.y = y;
    colHullX.size.width = width;
    colHullX.size.height = height;
    colHullY.origin.x = x;
    colHullY.origin.y = y;
    colHullY.size.width = width;
    colHullY.size.height = height;
}

// - (void) updateMotion;
void FlxObject::updateMotion() {
    if (!moves) return;
    if (solid) refreshHulls();
    onFloor = false;
    float vc = 0;
    vc = (FlxU::computeVelocityWithParam1(angularVelocity, angularAcceleration, angularDrag, maxAngular) - angularVelocity) / 2;
    angularVelocity += vc;
    angle = fmod(angle + angularVelocity * FlxG::elapsed(), 360);  // double fmod, as in the original
    angularVelocity += vc;
    CGPoint thrustComponents;
    if (thrust != 0) {
        thrustComponents = FlxU::rotatePointWithParam1(-thrust, 0, 0, 0, angle);
        CGPoint maxComponents = FlxU::rotatePointWithParam1(-maxThrust, 0, 0, 0, angle);
        float max = ((maxComponents.x > 0) ? maxComponents.x : -maxComponents.x);
        if (max > ((maxComponents.y > 0) ? maxComponents.y : -maxComponents.y))
            maxComponents.y = max;
        else
            max = ((maxComponents.y > 0) ? maxComponents.y : -maxComponents.y);
        maxVelocity.x = maxVelocity.y = ((max > 0) ? max : -max);
    } else
        thrustComponents = CGPointZero;
    vc = (FlxU::computeVelocityWithParam1(velocity.x, acceleration.x + thrustComponents.x, drag.x, maxVelocity.x) - velocity.x) / 2;
    velocity.x += vc;
    float xd = velocity.x * FlxG::elapsed();
    velocity.x += vc;
    vc = (FlxU::computeVelocityWithParam1(velocity.y, acceleration.y + thrustComponents.y, drag.y, maxVelocity.y) - velocity.y) / 2;
    velocity.y += vc;
    float yd = velocity.y * FlxG::elapsed();
    velocity.y += vc;
    x += xd;
    y += yd;
    if (!solid) return;
    colVector.x = xd;
    colVector.y = yd;
    colHullX.size.width += ((colVector.x > 0) ? colVector.x : -colVector.x);
    if (colVector.x < 0) colHullX.origin.x += colVector.x;
    colHullY.origin.x = x;
    colHullY.size.height += ((colVector.y > 0) ? colVector.y : -colVector.y);
    if (colVector.y < 0) colHullY.origin.y += colVector.y;
}

// - (void) updateFlickering;
void FlxObject::updateFlickering() {
    if (flickering()) {
        if (_flickerTimer > 0) {
            _flickerTimer -= FlxG::elapsed();
            if (_flickerTimer == 0) _flickerTimer = -1;
        }
        if (_flickerTimer < 0)
            flicker(-1);
        else {
            _flicker = !_flicker;
            visible = !_flicker;
        }
    }
}

// - (void) update;
void FlxObject::update() {
    updateMotion();
    updateFlickering();
}

// - (BOOL) overlaps:(FlxObject *)Object;
BOOL FlxObject::overlaps(FlxObject* Object) {
    CGPoint _point = getScreenXY();
    float tx = _point.x;
    float ty = _point.y;
    _point = Object->getScreenXY();
    if ((_point.x <= tx - Object->getWidth()) || (_point.x >= tx + width) || (_point.y <= ty - Object->getHeight()) ||
        (_point.y >= ty + height))
        return false;
    return true;
}

// - (BOOL) overlapsPointWithParam1:(float)X param2:(float)Y param3:(BOOL)PerPixel;
BOOL FlxObject::overlapsPointWithParam1(float X, float Y, BOOL PerPixel) {
    X = X - FlxU::floor(FlxG::scroll()->x);
    Y = Y - FlxU::floor(FlxG::scroll()->y);
    CGPoint _point = getScreenXY();
    if ((X <= _point.x) || (X >= _point.x + width) || (Y <= _point.y) || (Y >= _point.y + height)) return false;
    return true;
}

// - (BOOL) collide:(FlxObject *)Object;
BOOL FlxObject::collide(FlxObject* Object) { return FlxU::collideWithParam1(this, (Object == nullptr) ? this : Object); }

// - (void) hitLeftWithParam1:(FlxObject *)Contact param2:(float)Velocity;
void FlxObject::hitLeftWithParam1(FlxObject* Contact, float Velocity) {
    if (!fixed) velocity.x = Velocity;
}
// - (void) hitRightWithParam1:(FlxObject *)Contact param2:(float)Velocity;
void FlxObject::hitRightWithParam1(FlxObject* Contact, float Velocity) { hitLeftWithParam1(Contact, Velocity); }
// - (void) hitTopWithParam1:(FlxObject *)Contact param2:(float)Velocity;
void FlxObject::hitTopWithParam1(FlxObject* Contact, float Velocity) {
    if (!fixed) velocity.y = Velocity;
}
// - (void) hitBottomWithParam1:(FlxObject *)Contact param2:(float)Velocity;
void FlxObject::hitBottomWithParam1(FlxObject* Contact, float Velocity) {
    onFloor = true;
    if (!fixed) velocity.y = Velocity;
}
// - (void) hurt:(float)Damage;
void FlxObject::hurt(float Damage) {
    if ((health -= Damage) <= 0) kill();
}
// - (void) kill;
void FlxObject::kill() {
    exists = false;
    dead = true;
}
// - (void) flicker:(float)Duration;
void FlxObject::flicker(float Duration) {
    _flickerTimer = Duration;
    if (_flickerTimer < 0) {
        _flicker = false;
        visible = true;
    }
}
// - (BOOL) flickering;
BOOL FlxObject::flickering() { return _flickerTimer >= 0; }

// - (CGPoint) getScreenXY
CGPoint FlxObject::getScreenXY() {
    CGPoint Point;
    Point.x = std::floor(x + FlxU::roundingError()) + std::floor(FlxG::scroll()->x * scrollFactor.x);
    Point.y = std::floor(y + FlxU::roundingError()) + std::floor(FlxG::scroll()->y * scrollFactor.y);
    return Point;
}

// - (BOOL) onScreen;
BOOL FlxObject::onScreen() {
    CGPoint _point = getScreenXY();
    if ((_point.x + width < 0) || (_point.x > FlxG::width()) || (_point.y + height < 0) || (_point.y > FlxG::height())) return false;
    return true;
}

// - (void) resetWithParam1:(float)X param2:(float)Y;
void FlxObject::resetWithParam1(float X, float Y) {
    x = X;
    y = Y;
    exists = true;
    dead = false;
}

// ============================== FlxGroup ==============================

// - (id) initWithX:(float)X y:(float)Y width:(float)Width height:(float)Height;
FlxGroup* FlxGroup::initWithX(float X, float Y, float Width, float Height) {
    FlxObject::initWithX(X, Y, Width, Height);
    _group = true;
    solid = false;
    members.clear();
    _last = CGPointZero;
    _first = true;
    return this;
}

// - (FlxObject *) addWithParam1:(FlxObject *)Object param2:(BOOL)ShareScroll;
FlxObject* FlxGroup::addWithParam1(FlxObject* Object, BOOL ShareScroll) {
    members.push_back(Object);  // NSMutableArray push: (nil -> NSNull; not reachable here)
    if (ShareScroll) Object->scrollFactor = scrollFactor;
    return Object;
}

// - (FlxObject *) replaceWithParam1:(FlxObject *)OldObject param2:(FlxObject *)NewObject;
FlxObject* FlxGroup::replaceWithParam1(FlxObject* OldObject, FlxObject* NewObject) {
    for (size_t i = 0; i < members.size(); i++)
        if (members[i] == OldObject) {
            members[i] = NewObject;
            return NewObject;
        }
    return nullptr;
}

// - (FlxObject *) replaceObjectAtIndex:(unsigned int)index withObject:(FlxObject *)object;
FlxObject* FlxGroup::replaceObjectAtIndex(unsigned int index, FlxObject* NewObject) {
    if (index >= members.size()) return nullptr;
    members[index] = NewObject;
    return NewObject;
}

// - (FlxObject *) remove:(FlxObject *)Object;
FlxObject* FlxGroup::remove(FlxObject* Object) {
    for (size_t i = 0; i < members.size(); i++)
        if (members[i] == Object) {
            members.erase(members.begin() + i);
            return Object;
        }
    return nullptr;
}

// - (FlxObject *) getFirstAvail;
FlxObject* FlxGroup::getFirstAvail() {
    for (auto* o : members)
        if (!o->exists) return o;
    return nullptr;
}
// - (BOOL) resetFirstAvailWithParam1:(float)X param2:(float)Y;
BOOL FlxGroup::resetFirstAvailWithParam1(float X, float Y) {
    FlxObject* o = getFirstAvail();
    if (o == nullptr) return false;
    o->resetWithParam1(X, Y);
    return true;
}
// - (FlxObject *) getFirstExtant;
FlxObject* FlxGroup::getFirstExtant() {
    for (auto* o : members)
        if (o->exists) return o;
    return nullptr;
}
// - (FlxObject *) getFirstAlive;
FlxObject* FlxGroup::getFirstAlive() {
    for (auto* o : members)
        if (!o->dead) return o;
    return nullptr;
}
// - (FlxObject *) getFirstDead;
FlxObject* FlxGroup::getFirstDead() {
    for (auto* o : members)
        if (o->dead) return o;
    return nullptr;
}
// - (int) countLiving;
int FlxGroup::countLiving() {
    int count = -1;
    for (auto* o : members)
        if (!o->dead) {
            if (count < 0) count = 0;
            count++;
        }
    return count;
}
// - (int) countDead;
int FlxGroup::countDead() {
    int count = -1;
    for (auto* o : members)
        if (o->dead) {
            if (count < 0) count = 0;
            count++;
        }
    return count;
}
// - (FlxObject *) getRandom;
FlxObject* FlxGroup::getRandom() { return members[(size_t)FlxU::floor(FlxU::random() * members.size())]; }

// - (void) saveOldPosition;
void FlxGroup::saveOldPosition() {
    if (_first) {
        _first = false;
        _last.x = 0;
        _last.y = 0;
        return;
    }
    _last.x = x;
    _last.y = y;
}

// - (void) updateMembers;
void FlxGroup::updateMembers() {
    float mx = 0, my = 0;
    BOOL moved = false;
    if ((x != _last.x) || (y != _last.y)) {
        moved = true;
        mx = x - _last.x;
        my = y - _last.y;
    }
    size_t l = members.size();
    for (size_t i = 0; i < l; i++) {
        FlxObject* o = members[i];
        if ((o != nullptr) && o->exists) {
            if (moved) {
                if (dynamic_cast<FlxGroup*>(o))
                    o->resetWithParam1(o->x + mx, o->y + my);
                else {
                    o->setX(o->x + mx);
                    o->setY(o->y + my);
                }
            }
            if (o->active) o->update();
            if (moved && o->solid) {
                CGRect o_colHullX = o->colHullX;
                o_colHullX.size.width += ((mx > 0) ? mx : -mx);
                if (mx < 0) o_colHullX.origin.x += mx;
                o->colHullX = o_colHullX;
                CGRect o_colHullY = o->colHullY;
                o_colHullY.origin.x = x;
                o_colHullY.size.height += ((my > 0) ? my : -my);
                if (my < 0) o_colHullY.origin.y += my;
                o->colHullY = o_colHullY;
                CGPoint o_colVector = o->colVector;
                o_colVector.x += mx;
                o_colVector.y += my;
                o->colVector = o_colVector;
            }
        }
    }
}

// - (void) update;
void FlxGroup::update() {
    saveOldPosition();
    updateMotion();
    updateMembers();
    updateFlickering();
}

// - (void) setX:(float)newX
void FlxGroup::setX(float newX) {
    float mx = newX - x;
    for (auto* o : members) {
        if (o->exists) {
            if (dynamic_cast<FlxGroup*>(o))
                o->resetWithParam1(o->x + mx, o->y);
            else
                o->setX(o->x + mx);
        }
    }
    FlxRect::setX(newX);
}

// - (void) setY:(float)newY   (sic: resets groups with o.y as X, as in the original)
void FlxGroup::setY(float newY) {
    float my = newY - y;
    for (auto* o : members) {
        if (o->exists) {
            if (dynamic_cast<FlxGroup*>(o))
                o->resetWithParam1(o->y, o->y + my);
            else
                o->setY(o->y + my);
        }
    }
    FlxRect::setY(newY);
}

// - (void) renderMembers;
void FlxGroup::renderMembers() {
    for (size_t i = 0; i < members.size(); i++) {
        FlxObject* o = members[i];
        if ((o != nullptr) && o->exists && o->visible) o->render();
    }
}
void FlxGroup::render() { renderMembers(); }

// - (void) killMembers;
void FlxGroup::killMembers() {
    size_t l = members.size();
    for (size_t i = 0; i < l; i++)
        if (members[i] != nullptr) members[i]->kill();
}
void FlxGroup::kill() {
    killMembers();
    FlxObject::kill();
}

// - (void) destroyMembers;
void FlxGroup::destroyMembers() {
    size_t l = members.size();
    for (size_t i = 0; i < l; i++)
        if (members[i] != nullptr) members[i]->destroy();
    members.clear();
}
void FlxGroup::destroy() {
    destroyMembers();
    FlxObject::destroy();
}

// - (void) resetWithParam1:(float)X param2:(float)Y;
void FlxGroup::resetWithParam1(float X, float Y) {
    saveOldPosition();
    FlxObject::resetWithParam1(X, Y);
    float mx = 0, my = 0;
    BOOL moved = false;
    if ((x != _last.x) || (y != _last.y)) {
        moved = true;
        mx = x - _last.x;
        my = y - _last.y;
    }
    size_t l = members.size();
    for (size_t i = 0; i < l; i++) {
        FlxObject* o = members[i];
        if ((o != nullptr) && o->exists) {
            if (moved) {
                if (dynamic_cast<FlxGroup*>(o))
                    o->resetWithParam1(o->x + mx, o->y + my);
                else {
                    o->setX(o->x + mx);
                    o->setY(o->y + my);
                    if (solid) {
                        CGRect o_colHullX = o->colHullX;
                        o_colHullX.size.width += ((mx > 0) ? mx : -mx);
                        if (mx < 0) o_colHullX.origin.x += mx;
                        o->colHullX = o_colHullX;
                        CGRect o_colHullY = o->colHullY;
                        o_colHullY.origin.x = x;
                        o_colHullY.size.height += ((my > 0) ? my : -my);
                        if (my < 0) o_colHullY.origin.y += my;
                        o->colHullY = o_colHullY;
                        CGPoint o_colVector = o->colVector;
                        o_colVector.x += mx;
                        o_colVector.y += my;
                        o->colVector = o_colVector;
                    }
                }
            }
        }
    }
}
