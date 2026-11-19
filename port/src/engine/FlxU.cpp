//
//  FlxU.cpp — FlxU.m, FlxGLView (texture-scale statics), FlxQuadTree.m, FlxList.m
//
//  Copyright Semi Secret Software 2009-2010. All rights reserved.
//  C++ port: MIT license (see ENGINE_LICENSE.txt).
//
#include <SDL3/SDL.h>

#include <cstdlib>
#include <random>
#include <set>

#include "Flixel.h"

static float _seed;
static float _originalSeed;
static float roundingError_ = 1e-07f;

float FlxU::roundingError() { return roundingError_; }

// + (void) openURL:(NSString *)URL;
void FlxU::openURL(const std::string& URL) { SDL_OpenURL(URL.c_str()); }

// + (float) floor:(float)N;
float FlxU::floor(float N) {
    float n = ((int)(N));
    return (N > 0) ? (n) : ((n != N) ? (n - 1) : (n));
}
// + (float) ceil:(float)N;
float FlxU::ceil(float N) {
    float n = ((int)(N));
    return (N > 0) ? ((n != N) ? (n + 1) : (n)) : (n);
}

// arc4random() replacement
// PORT: the engine stream is separate from the game's alien RNG (game/Aliens.cpp). Normally it is
// seeded from std::random_device like arc4random; with ZIGGURAT_SEED (test/practice determinism)
// it gets a fixed seed derived from it, so FlxQuake/FlxEmitter randomness is reproducible too.
static uint32_t arc4random_() {
    static std::mt19937 rng = [] {
        const char* s = std::getenv("ZIGGURAT_SEED");
        return s ? std::mt19937((uint32_t)std::strtoul(s, nullptr, 10) ^ 0x9E3779B9u) : std::mt19937(std::random_device{}());
    }();
    return rng();
}
#ifndef RAND_MAX_IOS
#define RAND_MAX_IOS 0x7fffffff  // RAND_MAX on iOS
#endif
// + (float) random:(BOOL)UseGlobalSeed;
float FlxU::random(BOOL UseGlobalSeed) {
    return (arc4random_() % ((unsigned)RAND_MAX_IOS + 1)) / ((float)(RAND_MAX_IOS) + 1);
}
// + (float) randomize:(float)Seed;   (integer arithmetic exactly as written)
float FlxU::randomize(float Seed) { return ((69621 * ((int)(Seed * 0x7FFFFFFF))) % 0x7FFFFFFF) / 0x7FFFFFFF; }
// + (float) mutateWithParam1:(float)Seed param2:(float)Mutator;
float FlxU::mutateWithParam1(float Seed, float Mutator) {
    Seed += Mutator;
    if (Seed > 1) Seed -= ((int)(Seed));
    return Seed;
}
float FlxU::seed() { return _originalSeed; }
void FlxU::setSeed(float Seed) {
    _seed = Seed;
    _originalSeed = _seed;
}

// + (CGPoint) rotatePointWithParam1:param2:param3:param4:param5:
CGPoint FlxU::rotatePointWithParam1(float X, float Y, float PivotX, float PivotY, float Angle) {
    CGPoint P;
    float radians = -Angle / 180 * M_PI;
    float dx = X - PivotX;
    float dy = PivotY - Y;
    P.x = PivotX + cos(radians) * dx - sin(radians) * dy;
    P.y = PivotY - (sin(radians) * dx + cos(radians) * dy);
    return P;
}
// + (float) getAngleWithParam1:(float)X param2:(float)Y;
float FlxU::getAngleWithParam1(float X, float Y) { return atan2(Y, X) * 180 / M_PI; }

// + (float) computeVelocityWithParam1:(float)Velocity param2:(float)Acceleration param3:(float)Drag param4:(float)Max;
float FlxU::computeVelocityWithParam1(float Velocity, float Acceleration, float Drag, float Max) {
    if (Acceleration != 0)
        Velocity += Acceleration * FlxG::elapsed();
    else if (Drag != 0) {
        float d = Drag * FlxG::elapsed();
        if (Velocity - d > 0)
            Velocity -= d;
        else if (Velocity + d < 0)
            Velocity += d;
        else
            Velocity = 0;
    }
    if ((Velocity != 0) && (Max != 10000)) {
        if (Velocity > Max)
            Velocity = Max;
        else if (Velocity < -Max)
            Velocity = -Max;
    }
    return Velocity;
}

// + (void) setWorldBoundsWithParam1:param2:param3:param4:  (quad-tree bounds commented out in original)
void FlxU::setWorldBoundsWithParam1(float X, float Y, float Width, float Height) {
    if ((X == 0) && (Y == 0) && (Width == 0) && (Height == 0)) {
        X = -128 * FlxG::width();
        Y = -128 * FlxG::height();
        Width = 256 * FlxG::width();
        Height = 256 * FlxG::height();
    }
}

// + (BOOL) overlapWithParam1:(FlxObject *)Object1 param2:(FlxObject *)Object2 target:(id)target action:(SEL)action
BOOL FlxU::overlapWithParam1(FlxObject* Object1, FlxObject* Object2) {
    if ((Object1 == nullptr) || !Object1->exists || (Object2 == nullptr) || !Object2->exists) return false;
    SDL_Log("FlxU overlap");
    return false;  // body commented out in flixel-ios
}

// + (BOOL) collideWithParam1:(FlxObject *)Object1 param2:(FlxObject *)Object2;  (quad tree commented out)
BOOL FlxU::collideWithParam1(FlxObject* Object1, FlxObject* Object2) {
    if ((Object1 == nullptr) || !Object1->exists || (Object2 == nullptr) || !Object2->exists) return false;
    return false;
}

// flatten groups (NSMutableSet anyObject order is unspecified; we keep insertion order)
static void flatten(FlxObject* root, std::vector<FlxObject*>& out) {
    std::vector<FlxObject*> toAdd{root};
    std::set<FlxObject*> seen;
    while (!toAdd.empty()) {
        FlxObject* obj = toAdd.back();
        toAdd.pop_back();
        if (auto* g = dynamic_cast<FlxGroup*>(obj)) {
            for (auto* m : g->members) toAdd.push_back(m);
        } else if (seen.insert(obj).second)
            out.push_back(obj);
    }
}

// + (BOOL) alternateCollideWithParam1:(FlxObject *)Object1 param2:(FlxObject *)Object2;
BOOL FlxU::alternateCollideWithParam1(FlxObject* Object1, FlxObject* Object2) {
    if (Object1 == nullptr || !Object1->exists || Object2 == nullptr || !Object2->exists) return false;
    std::vector<FlxObject*> set1, set2;
    flatten(Object1, set1);
    flatten(Object2, set2);
    BOOL c = false;
    for (auto* obj1 : set1) {
        for (auto* obj2 : set2) {
            if (obj1 == obj2 || !obj1->exists || !obj2->exists || !obj1->solid || !obj2->solid ||
                (obj1->x + obj1->getWidth() < obj2->x + roundingError_) || (obj1->x + roundingError_ > obj2->x + obj2->getWidth()) ||
                (obj1->y + obj1->getHeight() < obj2->y + roundingError_) || (obj1->y + roundingError_ > obj2->y + obj2->getHeight()))
                continue;
            c |= solveXCollisionWithParam1(obj1, obj2);
            c |= solveYCollisionWithParam1(obj1, obj2);
        }
    }
    return c;
}

// + (BOOL) collideObject:(FlxObject *)object withGroup:(FlxGroup *)group;
BOOL FlxU::collideObject(FlxObject* object, FlxGroup* group) {
    if (object == nullptr || !object->exists || group == nullptr || !group->exists || !object->solid) return false;
    BOOL c = false;
    float objectLeft = object->x;
    float objectRight = object->x + object->getWidth();
    float objectTop = object->y;
    float objectBottom = object->y + object->getHeight();
    std::vector<FlxObject*> toCollide{group};
    std::set<FlxObject*> seen;
    while (!toCollide.empty()) {
        FlxObject* groupObject = toCollide.back();
        toCollide.pop_back();
        if (!seen.insert(groupObject).second) continue;  // NSMutableSet semantics
        if (auto* g = dynamic_cast<FlxGroup*>(groupObject)) {
            for (auto* m : g->members) toCollide.push_back(m);
        } else if (!(object == groupObject || !groupObject->exists || !groupObject->solid || objectRight < groupObject->x ||
                     objectLeft > groupObject->x + groupObject->getWidth() || objectBottom < groupObject->y ||
                     objectTop > groupObject->y + groupObject->getHeight())) {
            c |= solveXCollisionWithParam1(object, groupObject);
            c |= solveYCollisionWithParam1(object, groupObject);
        }
    }
    return c;
}

// + (BOOL) solveCollisionWithParam1:(FlxObject *)Object1 param2:(FlxObject *)Object2;
BOOL FlxU::solveCollisionWithParam1(FlxObject* Object1, FlxObject* Object2) {
    BOOL cx = solveXCollisionWithParam1(Object1, Object2);
    BOOL cy = solveYCollisionWithParam1(Object1, Object2);
    return cx || cy;
}

static inline float absf(float v) { return (v > 0) ? v : -v; }

// + (BOOL) solveXCollisionWithParam1:(FlxObject *)Object1 param2:(FlxObject *)Object2;
BOOL FlxU::solveXCollisionWithParam1(FlxObject* Object1, FlxObject* Object2) {
    BOOL debug = Object1->debug || Object2->debug;
    float o1 = Object1->colVector.x;
    float o2 = Object2->colVector.x;
    if (o1 == o2) return false;
    Object1->preCollide(Object2);
    Object2->preCollide(Object1);
    float overlap;
    BOOL hit = false;
    BOOL obj1Stopped = o1 == 0, obj1MoveNeg = o1 < 0, obj1MovePos = o1 > 0;
    BOOL obj2Stopped = o2 == 0, obj2MoveNeg = o2 < 0, obj2MovePos = o2 > 0;
    CGRect obj1Hull = Object1->colHullX;
    CGRect obj2Hull = Object2->colHullX;
    std::vector<CGPoint>& co1 = Object1->colOffsets;
    std::vector<CGPoint>& co2 = Object2->colOffsets;
    unsigned int l1 = (unsigned int)co1.size(), l2 = (unsigned int)co2.size();
    float ox1, oy1, ox2, oy2, r1, r2, sv1, sv2;
    BOOL p1hn2 = ((obj1Stopped && obj2MoveNeg) || (obj1MovePos && obj2Stopped) || (obj1MovePos && obj2MoveNeg) ||
                  (obj1MoveNeg && obj2MoveNeg && (absf(o1) < absf(o2))) || (obj1MovePos && obj2MovePos && (absf(o1) > absf(o2))));
    if (p1hn2 ? (!Object1->collideRight || !Object2->collideLeft) : (!Object1->collideLeft || !Object2->collideRight)) return false;
    for (unsigned int i1 = 0; i1 < l1; i1++) {
        CGPoint oc1 = co1[i1];
        ox1 = oc1.x;
        oy1 = oc1.y;
        obj1Hull.origin.x += ox1;
        obj1Hull.origin.y += oy1;
        for (unsigned int i2 = 0; i2 < l2; i2++) {
            CGPoint oc2 = co2[i2];
            ox2 = oc2.x;
            oy2 = oc2.y;
            obj2Hull.origin.x += ox2;
            obj2Hull.origin.y += oy2;
            if ((obj1Hull.origin.x + obj1Hull.size.width - obj2Hull.origin.x < roundingError_) ||
                (roundingError_ > obj2Hull.origin.x + obj2Hull.size.width - obj1Hull.origin.x) ||
                (obj1Hull.origin.y + obj1Hull.size.height - obj2Hull.origin.y < roundingError_) ||
                (roundingError_ > obj2Hull.origin.y + obj2Hull.size.height - obj1Hull.origin.y)) {
                obj2Hull.origin.x -= ox2;
                obj2Hull.origin.y -= oy2;
                continue;
            }
            if (p1hn2) {
                if (obj1MoveNeg) r1 = obj1Hull.origin.x + Object1->colHullY.size.width;
                else r1 = obj1Hull.origin.x + obj1Hull.size.width;
                if (obj2MoveNeg) r2 = obj2Hull.origin.x;
                else r2 = obj2Hull.origin.x + obj2Hull.size.width - Object2->colHullY.size.width;
            } else {
                if (obj2MoveNeg) r1 = -obj2Hull.origin.x - Object2->colHullY.size.width;
                else r1 = -obj2Hull.origin.x - obj2Hull.size.width;
                if (obj1MoveNeg) r2 = -obj1Hull.origin.x;
                else r2 = -obj1Hull.origin.x - obj1Hull.size.width + Object1->colHullY.size.width;
            }
            overlap = r1 - r2;
            if ((overlap == 0) || ((!Object1->fixed && absf(overlap) > obj1Hull.size.width * 0.8)) ||
                ((!Object2->fixed && absf(overlap) > obj2Hull.size.width * 0.8))) {
                obj2Hull.origin.x -= ox2;
                obj2Hull.origin.y -= oy2;
                continue;
            }
            hit = true;
            sv1 = Object2->velocity.x;
            sv2 = Object1->velocity.x;
            if (!Object1->fixed && Object2->fixed) {
                if (dynamic_cast<FlxGroup*>(Object1)) Object1->resetWithParam1(Object1->x - overlap, Object1->y);
                else Object1->setX(Object1->x - overlap);
            } else if (Object1->fixed && !Object2->fixed) {
                if (dynamic_cast<FlxGroup*>(Object2)) Object2->resetWithParam1(Object2->x + overlap, Object2->y);
                else Object2->setX(Object2->x + overlap);
            } else if (!Object1->fixed && !Object2->fixed) {
                overlap /= 2;
                if (dynamic_cast<FlxGroup*>(Object1)) Object1->resetWithParam1(Object1->x - overlap, Object1->y);
                else Object1->setX(Object1->x - overlap);
                if (dynamic_cast<FlxGroup*>(Object2)) Object2->resetWithParam1(Object2->x + overlap, Object2->y);
                else Object2->setX(Object2->x + overlap);
                sv1 /= 2;
                sv2 /= 2;
            }
            if (p1hn2) {
                if (debug) SDL_Log("hit right/left");
                Object1->hitRightWithParam1(Object2, sv1);
                Object2->hitLeftWithParam1(Object1, sv2);
            } else {
                if (debug) SDL_Log("hit right/left");
                Object1->hitLeftWithParam1(Object2, sv1);
                Object2->hitRightWithParam1(Object1, sv2);
            }
            if (!Object1->fixed && (overlap != 0)) {
                if (p1hn2) obj1Hull.size.width -= overlap;
                else {
                    obj1Hull.origin.x -= overlap;
                    obj1Hull.size.width += overlap;
                }
                Object1->colHullY.origin.x -= overlap;
            }
            if (!Object2->fixed && (overlap != 0)) {
                if (p1hn2) {
                    obj2Hull.origin.x += overlap;
                    obj2Hull.size.width -= overlap;
                } else
                    obj2Hull.size.width += overlap;
                Object2->colHullY.origin.x += overlap;
            }
            obj2Hull.origin.x -= ox2;
            obj2Hull.origin.y -= oy2;
        }
        obj1Hull.origin.x -= ox1;
        obj1Hull.origin.y -= oy1;
    }
    Object1->colHullX = obj1Hull;
    Object2->colHullX = obj2Hull;
    return hit;
}

// + (BOOL) solveYCollisionWithParam1:(FlxObject *)Object1 param2:(FlxObject *)Object2;
BOOL FlxU::solveYCollisionWithParam1(FlxObject* Object1, FlxObject* Object2) {
    float o1 = Object1->colVector.y;
    float o2 = Object2->colVector.y;
    if (o1 == o2) return false;
    Object1->preCollide(Object2);
    Object2->preCollide(Object1);
    float overlap;
    BOOL hit = false;
    BOOL obj1Stopped = o1 == 0, obj1MoveNeg = o1 < 0, obj1MovePos = o1 > 0;
    BOOL obj2Stopped = o2 == 0, obj2MoveNeg = o2 < 0, obj2MovePos = o2 > 0;
    CGRect obj1Hull = Object1->colHullY;
    CGRect obj2Hull = Object2->colHullY;
    std::vector<CGPoint>& co1 = Object1->colOffsets;
    std::vector<CGPoint>& co2 = Object2->colOffsets;
    unsigned int l1 = (unsigned int)co1.size(), l2 = (unsigned int)co2.size();
    float ox1, oy1, ox2, oy2, r1, r2, sv1, sv2;
    BOOL p1hn2 = ((obj1Stopped && obj2MoveNeg) || (obj1MovePos && obj2Stopped) || (obj1MovePos && obj2MoveNeg) ||
                  (obj1MoveNeg && obj2MoveNeg && (absf(o1) < absf(o2))) || (obj1MovePos && obj2MovePos && (absf(o1) > absf(o2))));
    if (p1hn2 ? (!Object1->collideBottom || !Object2->collideTop) : (!Object1->collideTop || !Object2->collideBottom)) return false;
    for (unsigned int i1 = 0; i1 < l1; i1++) {
        CGPoint oc1 = co1[i1];
        ox1 = oc1.x;
        oy1 = oc1.y;
        obj1Hull.origin.x += ox1;
        obj1Hull.origin.y += oy1;
        for (unsigned int i2 = 0; i2 < l2; i2++) {
            CGPoint oc2 = co2[i2];
            ox2 = oc2.x;
            oy2 = oc2.y;
            obj2Hull.origin.x += ox2;
            obj2Hull.origin.y += oy2;
            if ((obj1Hull.origin.x + obj1Hull.size.width - obj2Hull.origin.x < roundingError_) ||
                (roundingError_ > obj2Hull.origin.x + obj2Hull.size.width - obj1Hull.origin.x) ||
                (obj1Hull.origin.y + obj1Hull.size.height - obj2Hull.origin.y < roundingError_) ||
                (roundingError_ > obj2Hull.origin.y + obj2Hull.size.height - obj1Hull.origin.y)) {
                obj2Hull.origin.x -= ox2;
                obj2Hull.origin.y -= oy2;
                continue;
            }
            if (p1hn2) {
                if (obj1MoveNeg) r1 = obj1Hull.origin.y + Object1->colHullX.size.height;
                else r1 = obj1Hull.origin.y + obj1Hull.size.height;
                if (obj2MoveNeg) r2 = obj2Hull.origin.y;
                else r2 = obj2Hull.origin.y + obj2Hull.size.height - Object2->colHullX.size.height;
            } else {
                if (obj2MoveNeg) r1 = -obj2Hull.origin.y - Object2->colHullX.size.height;
                else r1 = -obj2Hull.origin.y - obj2Hull.size.height;
                if (obj1MoveNeg) r2 = -obj1Hull.origin.y;
                else r2 = -obj1Hull.origin.y - obj1Hull.size.height + Object1->colHullX.size.height;
            }
            overlap = r1 - r2;
            if ((overlap == 0) || ((!Object1->fixed && absf(overlap) > obj1Hull.size.height * 0.8)) ||
                ((!Object2->fixed && absf(overlap) > obj2Hull.size.height * 0.8))) {
                obj2Hull.origin.x -= ox2;
                obj2Hull.origin.y -= oy2;
                continue;
            }
            hit = true;
            sv1 = Object2->velocity.y;
            sv2 = Object1->velocity.y;
            if (!Object1->fixed && Object2->fixed) {
                if (dynamic_cast<FlxGroup*>(Object1)) Object1->resetWithParam1(Object1->x, Object1->y - overlap);
                else Object1->setY(Object1->y - overlap);
            } else if (Object1->fixed && !Object2->fixed) {
                if (dynamic_cast<FlxGroup*>(Object2)) Object2->resetWithParam1(Object2->x, Object2->y + overlap);
                else Object2->setY(Object2->y + overlap);
            } else if (!Object1->fixed && !Object2->fixed) {
                overlap /= 2;
                if (dynamic_cast<FlxGroup*>(Object1)) Object1->resetWithParam1(Object1->x, Object1->y - overlap);
                else Object1->setY(Object1->y - overlap);
                if (dynamic_cast<FlxGroup*>(Object2)) Object2->resetWithParam1(Object2->x, Object2->y + overlap);
                else Object2->setY(Object2->y + overlap);
                sv1 /= 2;
                sv2 /= 2;
            }
            if (p1hn2) {
                Object1->hitBottomWithParam1(Object2, sv1);
                Object2->hitTopWithParam1(Object1, sv2);
            } else {
                Object1->hitTopWithParam1(Object2, sv1);
                Object2->hitBottomWithParam1(Object1, sv2);
            }
            if (!Object1->fixed && (overlap != 0)) {
                if (p1hn2) {
                    obj1Hull.origin.y -= overlap;
                    if (Object2->fixed && Object2->moves) {
                        sv1 = Object2->colVector.x;
                        Object1->setX(Object1->x + sv1);
                        obj1Hull.origin.x += sv1;
                        Object1->colHullX.origin.x += sv1;
                    }
                } else {
                    obj1Hull.origin.y -= overlap;
                    obj1Hull.size.height += overlap;
                }
            }
            if (!Object2->fixed && (overlap != 0)) {
                if (p1hn2) {
                    obj2Hull.origin.y += overlap;
                    obj2Hull.size.height -= overlap;
                } else {
                    obj2Hull.size.height += overlap;
                    if (Object1->fixed && Object1->moves) {
                        sv2 = Object1->colVector.x;
                        Object2->setX(Object2->x + sv2);
                        obj2Hull.origin.x += sv2;
                        Object2->colHullX.origin.x += sv2;
                    }
                }
            }
            obj2Hull.origin.x -= ox2;
            obj2Hull.origin.y -= oy2;
        }
        obj1Hull.origin.x -= ox1;
        obj1Hull.origin.y -= oy1;
    }
    Object1->colHullY = obj1Hull;
    Object2->colHullY = obj2Hull;
    return hit;
}

// ============================== FlxGLView statics ==============================
static float textureScale_ = 1024.0f;
void FlxGLView::setTextureScale(float s) { textureScale_ = s; }
float FlxGLView::textureScale() { return textureScale_; }
// + (GLshort) convertToShort:(float)value   (sic: +0.5 applied after the cast)
GLshort FlxGLView::convertToShort(float value) { return (GLshort)(((GLshort)((value)*textureScale_)) + 0.5); }
float FlxGLView::convertFromShort(GLshort value) { return value * 1.0 / textureScale_; }

// ============================== FlxQuadTree ==============================
// PORT: the original pooled nodes with a custom alloc/retain/release; here nodes are plain heap
// objects freed with the tree. Static iteration state is kept as in the original.
static FlxObject* _o;
static float _ol, _ot, _or, _ob;
static unsigned int _oa;

FlxQuadTree::~FlxQuadTree() {
    for (FlxList* l : {_headA, _headB}) {
        while (l) {
            FlxList* n = l->next;
            delete l;
            l = n;
        }
    }
    delete _nw;
    delete _ne;
    delete _se;
    delete _sw;
}

static void copyList(FlxList* src, FlxList*& tail) {
    if (src && src->object != nullptr) {
        FlxList* itr = src;
        while (itr != nullptr) {
            if (tail->object != nullptr) {
                FlxList* ot = tail;
                tail = new FlxList;
                ot->next = tail;
            }
            tail->object = itr->object;
            itr = itr->next;
        }
    }
}

// - (id) initWithX:y:width:height:parent:  (-reInitWithX: body)
FlxQuadTree* FlxQuadTree::initWithX(float X, float Y, float Width, float Height, FlxQuadTree* Parent) {
    zoneDetach();  // PORT: quad trees delete their own children (see ~FlxQuadTree); not zone-owned
    x = X;
    y = Y;
    width = Width;
    height = Height;
    _canSubdivide = (Width > MIN()) || (Height > MIN());
    _headA = _tailA = new FlxList;
    _headB = _tailB = new FlxList;
    if (Parent) {
        copyList(Parent->_headA, _tailA);
        copyList(Parent->_headB, _tailB);
    }
    _nw = _ne = _se = _sw = nullptr;
    _l = x;
    _r = x + width;
    _hw = width / 2;
    _mx = _l + _hw;
    _t = y;
    _b = y + height;
    _hh = height / 2;
    _my = _t + _hh;
    return this;
}

// - (void) addWithParam1:(FlxObject *)Object param2:(unsigned int)List;
void FlxQuadTree::addWithParam1(FlxObject* Object, unsigned int List) {
    _oa = List;
    if (auto* g = dynamic_cast<FlxGroup*>(Object)) {
        size_t l = g->members.size();
        for (size_t i = 0; i < l; i++) {
            FlxObject* m = g->members[i];
            if ((m != nullptr) && m->exists) {
                if (dynamic_cast<FlxGroup*>(m))
                    addWithParam1(m, List);
                else if (m->solid) {
                    _o = m;
                    _ol = _o->x;
                    _ot = _o->y;
                    _or = _o->x + _o->getWidth();
                    _ob = _o->y + _o->getHeight();
                    addObject();
                }
            }
        }
    }
    if (Object->solid) {
        _o = Object;
        _ol = _o->x;
        _ot = _o->y;
        _or = _o->x + _o->getWidth();
        _ob = _o->y + _o->getHeight();
        addObject();
    }
}

// - (void) addObject;
void FlxQuadTree::addObject() {
    if (!_canSubdivide || ((_l >= _ol) && (_r <= _or) && (_t >= _ot) && (_b <= _ob))) {
        addToList();
        return;
    }
    if ((_ol > _l) && (_or < _mx)) {
        if ((_ot > _t) && (_ob < _my)) {
            if (_nw == nullptr) _nw = (new FlxQuadTree)->initWithX(_l, _t, _hw, _hh, this);
            _nw->addObject();
            return;
        }
        if ((_ot > _my) && (_ob < _b)) {
            if (_sw == nullptr) _sw = (new FlxQuadTree)->initWithX(_l, _my, _hw, _hh, this);
            _sw->addObject();
            return;
        }
    }
    if ((_ol > _mx) && (_or < _r)) {
        if ((_ot > _t) && (_ob < _my)) {
            if (_ne == nullptr) _ne = (new FlxQuadTree)->initWithX(_mx, _t, _hw, _hh, this);
            _ne->addObject();
            return;
        }
        if ((_ot > _my) && (_ob < _b)) {
            if (_se == nullptr) _se = (new FlxQuadTree)->initWithX(_mx, _my, _hw, _hh, this);
            _se->addObject();
            return;
        }
    }
    if ((_or > _l) && (_ol < _mx) && (_ob > _t) && (_ot < _my)) {
        if (_nw == nullptr) _nw = (new FlxQuadTree)->initWithX(_l, _t, _hw, _hh, this);
        _nw->addObject();
    }
    if ((_or > _mx) && (_ol < _r) && (_ob > _t) && (_ot < _my)) {
        if (_ne == nullptr) _ne = (new FlxQuadTree)->initWithX(_mx, _t, _hw, _hh, this);
        _ne->addObject();
    }
    if ((_or > _mx) && (_ol < _r) && (_ob > _my) && (_ot < _b)) {
        if (_se == nullptr) _se = (new FlxQuadTree)->initWithX(_mx, _my, _hw, _hh, this);
        _se->addObject();
    }
    if ((_or > _l) && (_ol < _mx) && (_ob > _my) && (_ot < _b)) {
        if (_sw == nullptr) _sw = (new FlxQuadTree)->initWithX(_l, _my, _hw, _hh, this);
        _sw->addObject();
    }
}

// - (void) addToList;
void FlxQuadTree::addToList() {
    FlxList*& tail = (_oa == A_LIST()) ? _tailA : _tailB;
    if (tail->object != nullptr) {
        FlxList* ot = tail;
        tail = new FlxList;
        ot->next = tail;
    }
    tail->object = _o;
    if (!_canSubdivide) return;
    if (_nw != nullptr) _nw->addToList();
    if (_ne != nullptr) _ne->addToList();
    if (_se != nullptr) _se->addToList();
    if (_sw != nullptr) _sw->addToList();
}

// - (BOOL) overlapWithParam1:(BOOL)BothLists target:(id)Target action:(SEL)Action;
BOOL FlxQuadTree::overlapWithParam1(BOOL BothLists, std::function<BOOL(FlxObject*, FlxObject*)> Target) {
    target = Target;
    BOOL c = false;
    FlxList* itr;
    if (BothLists) {
        _oa = B_LIST();
        if (_headA->object != nullptr) {
            itr = _headA;
            while (itr != nullptr) {
                _o = itr->object;
                if (_o->exists && _o->solid && overlapNode()) c = true;
                itr = itr->next;
            }
        }
        _oa = A_LIST();
        if (_headB->object != nullptr) {
            itr = _headB;
            while (itr != nullptr) {
                _o = itr->object;
                if (_o->exists && _o->solid) {
                    if ((_nw != nullptr) && _nw->overlapNode()) c = true;
                    if ((_ne != nullptr) && _ne->overlapNode()) c = true;
                    if ((_se != nullptr) && _se->overlapNode()) c = true;
                    if ((_sw != nullptr) && _sw->overlapNode()) c = true;
                }
                itr = itr->next;
            }
        }
    } else {
        if (_headA->object != nullptr) {
            itr = _headA;
            while (itr != nullptr) {
                _o = itr->object;
                if (_o->exists && _o->solid && overlapNode(itr->next)) c = true;
                itr = itr->next;
            }
        }
    }
    if ((_nw != nullptr) && _nw->overlapWithParam1(BothLists, target)) c = true;
    if ((_ne != nullptr) && _ne->overlapWithParam1(BothLists, target)) c = true;
    if ((_se != nullptr) && _se->overlapWithParam1(BothLists, target)) c = true;
    if ((_sw != nullptr) && _sw->overlapWithParam1(BothLists, target)) c = true;
    return c;
}

// - (BOOL) overlapNode:(FlxList *)Iterator;
BOOL FlxQuadTree::overlapNode(FlxList* Iterator) {
    BOOL c = false;
    FlxList* itr = Iterator;
    if (itr == nullptr) itr = (_oa == A_LIST()) ? _headA : _headB;
    if (itr->object != nullptr) {
        while (itr != nullptr) {
            FlxObject* co = itr->object;
            float re = FlxU::roundingError();
            if ((_o == co) || !co->exists || !_o->exists || !co->solid || !_o->solid || (_o->x + _o->getWidth() < co->x + re) ||
                (_o->x + re > co->x + co->getWidth()) || (_o->y + _o->getHeight() < co->y + re) || (_o->y + re > co->y + co->getHeight())) {
                itr = itr->next;
                continue;
            }
            if (!target) {
                _o->kill();
                co->kill();
                c = true;
            } else if (target(_o, co))
                c = true;
            itr = itr->next;
        }
    }
    return c;
}
