//
//  Flixel.h  —  C++17 translation of flixel-ios (Headers/Flixel, Headers/Flash, Headers/SemiSecret)
//
//  Copyright Semi Secret Software 2009-2010. All rights reserved.
//  C++ port for the ZiGGURAT revival project. Licensed under the MIT license (see ENGINE_LICENSE.txt).
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//
//  Translation conventions
//  - Two-phase construction mirrors [[Class alloc] initWith...]: `(new FlxSprite)->initWithX(...)`.
//  - ObjC selectors are kept in a comment above every method; `fooWithParam1:param2:` -> fooWithParam1(a, b).
//  - Instance variables keep their names and are public (ObjC @public-style access from game code).
//    ObjC dot-syntax on *other* objects goes through the (virtual) setters below, because flixel
//    overrides several of them (FlxPoint setX: updates predictedX, FlxGroup setX: moves members, ...).
//  - CGFloat is float (32-bit iOS). Calls to libm functions that ObjC made on doubles (floor, fmod, cos...)
//    stay double-precision, as in the original.
//  - Retain/release is not modelled: objects are owned by whoever created them; nothing is freed on
//    state switch.  // PORT: memory is leaked across state switches (harmless for this game's size).
#pragma once

#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "platform/platform.h"

// ---------------------------------------------------------------------------------------------
// CoreGraphics / GL types used by the engine
// ---------------------------------------------------------------------------------------------
typedef int16_t GLshort;
typedef uint8_t GLubyte;
typedef bool BOOL;
typedef uint16_t unichar;

struct CGPoint { float x = 0, y = 0; };
struct CGSize { float width = 0, height = 0; };
struct CGRect { CGPoint origin; CGSize size; };
inline CGPoint CGPointMake(float x, float y) { return CGPoint{x, y}; }
inline CGSize CGSizeMake(float w, float h) { return CGSize{w, h}; }
inline CGRect CGRectMake(float x, float y, float w, float h) { return CGRect{{x, y}, {w, h}}; }
static const CGPoint CGPointZero{};
static const CGRect CGRectZero{};
// CoreGraphics semantics: min edges inclusive, max edges exclusive.
inline bool CGRectContainsPoint(const CGRect& r, const CGPoint& p) {
    return p.x >= r.origin.x && p.x < r.origin.x + r.size.width && p.y >= r.origin.y && p.y < r.origin.y + r.size.height;
}

class FlxObject;
class FlxGroup;
class FlxSprite;
class FlxState;
class FlxGame;
class FlxQuake;
class FlxFlash;
class FlxFade;
class FlxSound;
class FlxTouches;
class FlxText;
class SemiSecretTexture;
class SemiSecretFont;
class SemiSecretText;

// ---------------------------------------------------------------------------------------------
// Flash
// ---------------------------------------------------------------------------------------------
class FlashObject {
public:
    virtual ~FlashObject() {}
};

class FlashPoint : public FlashObject {
public:
    float x = 0, y = 0;
    // - (id) initWithX:(float)X y:(float)Y
    FlashPoint* initWithX(float X = 0, float Y = 0) { x = X; y = Y; return this; }
    FlashPoint* add(FlashPoint* v);
    static float distanceWithParam1(FlashPoint* pt1, FlashPoint* pt2);
    bool isEqualToPoint(FlashPoint* p) { return x == p->x && y == p->y; }
    static FlashPoint* interpolateWithParam1(FlashPoint* pt1, FlashPoint* pt2, float f);
    float length();
    void normalize(float thickness);
    void offsetWithParam1(float dx, float dy) { x += dx; y += dy; }
    static FlashPoint* polarWithParam1(float len, float angle);
    FlashPoint* subtract(FlashPoint* v);
};

class FlashRectangle : public FlashObject {
public:
    float x = 0, y = 0, width = 0, height = 0;
    FlashRectangle* initWithX(float X = 0, float Y = 0, float W = 0, float H = 0) { x = X; y = Y; width = W; height = H; return this; }
    float bottom() { return y + height; }
    void setBottom(float b) { height = b - y; }
    float left() { return x; }
    void setLeft(float l) { x = l; }
    float right() { return x + width; }
    void setRight(float r) { width = r - x; }
    float top() { return y; }
    void setTop(float t) { y = t; }
    bool containsWithParam1(float X, float Y) { return x <= X && x + width >= X && y <= Y && y + height >= Y; }
    bool isEmpty() { return width > 0 && height > 0; }  // sic: inverted in the original
    void inflateWithParam1(float dx, float dy) { x -= dx; width += 2 * dx; y -= dy; height += 2 * dy; }
    void offsetWithParam1(float dx, float dy) { x += dx; y += dy; }
    void setEmpty() { x = y = width = height = 0; }
    FlashRectangle* intersection(FlashRectangle* r);
    bool intersects(FlashRectangle* r);
    FlashRectangle* union_(FlashRectangle* r);
};

// PORT: FlashFunction wrapped target/action/NSInvocation; here it wraps a std::function.
class FlashFunction : public FlashObject {
public:
    std::function<void()> fn;
    // + (FlashFunction *) functionWithTarget:(id)target action:(SEL)action;
    static FlashFunction* functionWith(std::function<void()> f) { auto* r = new FlashFunction; r->fn = std::move(f); return r; }
    // - (id) execute;
    void execute() { if (fn) fn(); }
};

// ---------------------------------------------------------------------------------------------
// NSUserDefaults replacement (used by FlxG audio settings and FlxSave)
// ---------------------------------------------------------------------------------------------
class UserDefaults {
public:
    static UserDefaults& standard();
    bool has(const std::string& key);
    std::string get(const std::string& key, const std::string& def = "");
    float getFloat(const std::string& key, float def = 0);
    bool getBool(const std::string& key, bool def = false);
    void set(const std::string& key, const std::string& value);
    void setFloat(const std::string& key, float v);
    void setBool(const std::string& key, bool v);
    void remove(const std::string& key);
    void registerDefault(const std::string& key, const std::string& value);
    std::vector<std::string> keysWithPrefix(const std::string& prefix);
    bool synchronize();
private:
    void load();
    bool loaded = false;
    std::map<std::string, std::string> values, defaults;
};

// ---------------------------------------------------------------------------------------------
// Flixel core
// ---------------------------------------------------------------------------------------------
// PORT (memory ownership): the original relied on retain/release, and FlxGame autoreleased the old
// state on -switchState:. Here every heap-allocated Flixel object registers in the "zone" of the
// state that was being built or running when it was allocated (FlxState::init makes its zone
// current). When a state is switched out it goes to a graveyard and, a few frames later (once no
// deferred callback can still reach it), its whole zone is deleted - see FlxGame::collectGraveyard.
// Stack/member objects never register (detected via the class operator new). Objects that manage
// their own children (FlxQuadTree) detach themselves.
class FlxPoint;
struct FlxZone {
    std::vector<FlxPoint*> objects;
    std::vector<std::string> uniqueTextures;  // FlxG texture-cache keys created as "unique" in this zone
    void freeAll();                           // deletes every registered object + the unique textures
};
extern FlxZone* gFlxZone;  // zone that newly allocated Flixel objects join (nullptr = global/permanent)

class FlxPoint {
public:
    float x = 0, y = 0, predictedX = 0, predictedY = 0;
    FlxZone* zone_ = nullptr;
    size_t zoneIndex_ = 0;
    FlxPoint();
    virtual ~FlxPoint();
    void zoneDetach();  // leave the zone (the object becomes self-managed)
    static void* operator new(size_t size);
    static void operator delete(void* p);
    // - (id) initWithX:(float)X y:(float)Y;
    FlxPoint* initWithPoint(float X = 0, float Y = 0) { x = X; y = Y; return this; }
    // - (void) setX: / setY:  (also sets predictedX/Y)
    virtual void setX(float X) { x = X; predictedX = x; }
    virtual void setY(float Y) { y = Y; predictedY = y; }
    float getX() const { return x; }
    float getY() const { return y; }
};

class FlxRect : public FlxPoint {
public:
    float width = 0, height = 0;
    // - (id) initWithX:(float)X y:(float)Y width:(float)Width height:(float)Height;
    FlxRect* initRect(float X, float Y, float Width = 0, float Height = 0) { initWithPoint(X, Y); width = Width; height = Height; return this; }
    virtual void setWidth(float W) { width = W; }
    virtual void setHeight(float H) { height = H; }
    virtual float getWidth() { return width; }
    virtual float getHeight() { return height; }
    float left() { return x; }
    float right() { return x + width; }
    float top() { return y; }
    float bottom() { return y + height; }
};

class FlxObject : public FlxRect {
public:
    BOOL exists = false, active = false, visible = false, solid = false, fixed = false;
    CGPoint velocity, acceleration, drag, maxVelocity;
    float angle = 0, angularVelocity = 0, angularAcceleration = 0, angularDrag = 0, maxAngular = 0;
    CGPoint origin;
    CGPoint rotationOrigin;  // ZiGGURAT engine extension: rotate about this point (if both != 0)
    float thrust = 0, maxThrust = 0;
    CGPoint scrollFactor;
    BOOL _flicker = false;
    float _flickerTimer = 0;
    float health = 0;
    BOOL dead = false, moves = false;
    CGRect colHullX, colHullY;
    CGPoint colVector;
    std::vector<CGPoint> colOffsets;
    BOOL _group = false;
    BOOL onFloor = false, collideLeft = false, collideRight = false, collideTop = false, collideBottom = false;
    BOOL debug = false, enableBlend = false;

    // - (id) initWithX:(float)X y:(float)y width:(float)width height:(float)height;
    virtual FlxObject* initWithX(float X, float Y, float Width, float Height);
    virtual void destroy() {}
    virtual void refreshHulls();
    virtual void update();
    virtual void render() {}
    virtual BOOL overlaps(FlxObject* Object);
    // - (BOOL) overlapsPointWithParam1:(float)X param2:(float)Y param3:(BOOL)PerPixel;
    virtual BOOL overlapsPointWithParam1(float X, float Y, BOOL PerPixel = false);
    BOOL collide(FlxObject* Object = nullptr);
    virtual void preCollide(FlxObject* Object) {}
    virtual void hitLeftWithParam1(FlxObject* Contact, float Velocity);
    virtual void hitRightWithParam1(FlxObject* Contact, float Velocity);
    virtual void hitTopWithParam1(FlxObject* Contact, float Velocity);
    virtual void hitBottomWithParam1(FlxObject* Contact, float Velocity);
    virtual void hurt(float Damage);
    virtual void kill();
    // - (void) flicker:(float)Duration;
    virtual void flicker(float Duration = 1);
    BOOL flickering();
    virtual CGPoint getScreenXY();
    virtual BOOL onScreen();
    // - (void) resetWithParam1:(float)X param2:(float)Y;
    virtual void resetWithParam1(float X, float Y);
    virtual void updateMotion();
    virtual void updateFlickering();
    virtual void onEmit() {}
};

class FlxGroup : public FlxObject {
public:
    std::vector<FlxObject*> members;
    CGPoint _last;
    BOOL _first = true;

    // - (id) init;
    virtual FlxGroup* init() { return initWithX(0, 0, 0, 0); }
    FlxGroup* initWithX(float X, float Y, float Width, float Height) override;
    // - (FlxObject *) add:(FlxObject *)Object;
    virtual FlxObject* add(FlxObject* Object) { return addWithParam1(Object, false); }
    virtual FlxObject* addWithParam1(FlxObject* Object, BOOL ShareScroll);
    FlxObject* replaceWithParam1(FlxObject* OldObject, FlxObject* NewObject);
    FlxObject* replaceObjectAtIndex(unsigned int index, FlxObject* NewObject);
    FlxObject* remove(FlxObject* Object);
    FlxObject* getFirstAvail();
    BOOL resetFirstAvailWithParam1(float X = 0, float Y = 0);
    FlxObject* getFirstExtant();
    FlxObject* getFirstAlive();
    FlxObject* getFirstDead();
    int countLiving();
    int countDead();
    FlxObject* getRandom();
    void saveOldPosition();
    virtual void updateMembers();
    void update() override;
    void setX(float newX) override;
    void setY(float newY) override;
    virtual void renderMembers();
    void render() override;
    virtual void killMembers();
    void kill() override;
    virtual void destroyMembers();
    void destroy() override;
    void resetWithParam1(float X, float Y) override;
    void removeAllObjects() { members.clear(); }
};

class FlxAnim {
public:
    std::string name;
    float delay = 0;
    std::vector<unsigned int> frames;
    BOOL looped = true;
    // - (id) initWithParam1:(NSString *)Name param2:(NSMutableArray *)Frames param3:(float)FrameRate param4:(BOOL)Looped;
    FlxAnim* initWithParam1(const std::string& Name, const std::vector<unsigned int>& Frames, float FrameRate = 0, BOOL Looped = true);
};

extern const unsigned int LEFT;
extern const unsigned int RIGHT;
extern const unsigned int UP;
extern const unsigned int DOWN;

class FlxSprite : public FlxObject {
public:
    ~FlxSprite() override;  // PORT: frees the FlxAnim objects it owns
    CGPoint offset;
    CGPoint scale;
    std::string blend;  // "" == nil, "screen"
    BOOL antialiasing = false;
    BOOL finished = false;
    float frameWidth = 0, frameHeight = 0;
    std::vector<FlxAnim*> _animations;
    unsigned int _flipped = 0;
    FlxAnim* _curAnim = nullptr;
    unsigned int _curFrame = 0;
    unsigned int _caf = 0;
    float _frameTimer = 0;
    FlashFunction* _callback = nullptr;
    // NB: every ivar starts at zero like ObjC +alloc; -init sets the real defaults afterwards
    // (and -fill: inside -init runs while _alpha is still 0, exactly as on the device).
    unsigned int _facing = 0;
    float _bakedRotation = 0;
    SemiSecretTexture* texture = nullptr;
    float _alpha = 0;
    unsigned int _color = 0;
    GLubyte red = 0, green = 0, blue = 0;
    GLubyte _colors[4 * 4] = {};
    GLshort _verticesUVs[4 * 2 + 4 * 2] = {};
    GLshort _uShort = 0, _vShort = 0, _uOffset = 0, _vOffset = 0;
    BOOL filled = false;
    BOOL useTextureVertexTexCoordData = false;
    float modelScale = 0;

    static unsigned int LEFT_() { return LEFT; }
    static unsigned int RIGHT_() { return RIGHT; }
    static unsigned int UP_() { return UP; }
    static unsigned int DOWN_() { return DOWN; }

    // + (id) spriteWithX:(float)X y:(float)Y graphic:(NSString *)SimpleGraphic;
    static FlxSprite* spriteWithX(float X, float Y, const std::string& SimpleGraphic);
    static FlxSprite* spriteWithGraphic(const std::string& SimpleGraphic) { return spriteWithX(0, 0, SimpleGraphic); }
    // - (id) init;  (and initWithX: / initWithX:y:)
    virtual FlxSprite* init() { return initWithX(0, 0, std::string(), 1.0f); }
    FlxSprite* initWithXY(float X, float Y) { return initWithX(X, Y, std::string(), 1.0f); }
    // - (id) initWithX:(float)X y:(float)Y graphic:(NSString *)SimpleGraphic modelScale:(float)ModelScale;
    virtual FlxSprite* initWithX(float X, float Y, const std::string& SimpleGraphic, float ModelScale = 1.0f);
    using FlxObject::initWithX;

    // - (FlxSprite *) loadGraphic:animated:reverse:width:height:unique:modelScale:
    FlxSprite* loadGraphic(const std::string& Graphic, BOOL Animated = false, BOOL Reverse = false, unsigned int Width = 0,
                           unsigned int Height = 0, BOOL Unique = false, float ModelScale = 1.0f);
    // - (FlxSprite *) loadGraphic:(NSString *)Graphic modelScale:(float)ModelScale;
    FlxSprite* loadGraphicModelScale(const std::string& Graphic, float ModelScale) { return loadGraphic(Graphic, false, false, 0, 0, false, ModelScale); }
    // - (FlxSprite *) loadGraphic:(NSString *)Graphic animated:(BOOL)Animated modelScale:(float)ModelScale;
    FlxSprite* loadGraphicAnimatedModelScale(const std::string& Graphic, BOOL Animated, float ModelScale) { return loadGraphic(Graphic, Animated, false, 0, 0, false, ModelScale); }
    // - (FlxSprite *) loadRotatedGraphic:rotations:frame:antiAliasing:autoBuffer:modelScale:
    FlxSprite* loadRotatedGraphic(const std::string& Graphic, unsigned int Rotations = 16, int Frame = -1, BOOL AntiAliasing = false,
                                  BOOL AutoBuffer = false, float ModelScale = 1.0f);
    // - (FlxSprite *) createGraphicWithWidth:height:color:unique:key:
    FlxSprite* createGraphicWithWidth(unsigned int Width, unsigned int Height, unsigned int Color = 0xffffffff, BOOL Unique = false,
                                      const std::string& Key = std::string());
    // APT binding: - (FlxSprite *) createGraphicWithParam1:param2:param3:param4:
    FlxSprite* createGraphicWithParam1(unsigned int Width, unsigned int Height, unsigned int Color = 0xffffffff, BOOL Unique = false) {
        return createGraphicWithWidth(Width, Height, Color, Unique);
    }
    void setWidth(float Width) override;
    void setHeight(float Height) override;
    void setFrameHeight(float h);
    void setFrameWidth(float w);
    void setupTexCoords();
    void setupVertices();
    void setupUVs() { setupVertices(); setupTexCoords(); }
    void resetHelpers();
    unsigned int facing() { return _facing; }
    void setFacing(unsigned int Direction);
    float alpha() { return _alpha; }
    void setAlpha(float Alpha);
    unsigned int color() { return _color; }
    virtual void setColor(unsigned int Color);
    // - (void) drawWithParam1:(FlxSprite *)Brush param2:(int)X param3:(int)Y;
    void drawWithParam1(FlxSprite* Brush, int X = 0, int Y = 0);
    void fill(unsigned int Color);
    void updateAnimation();
    void update() override;
    void renderSprite();
    void render() override;
    BOOL overlapsPointWithParam1(float X, float Y, BOOL PerPixel = false) override;
    // - (void) addAnimation:(NSString *)Name frames:(NSMutableArray *)Frames frameRate:(float)FrameRate looped:(BOOL)Looped;
    void addAnimation(const std::string& Name, const std::vector<unsigned int>& Frames, float FrameRate = 0, BOOL Looped = true);
    void addAnimationCallback(FlashFunction* AnimationCallback) { _callback = AnimationCallback; }
    // - (void) playWithParam1:(NSString *)AnimName param2:(BOOL)Force;
    void play(const std::string& AnimName, BOOL Force = false);
    void randomFrame();
    unsigned int frame() { return _caf; }
    void setFrame(unsigned int Frame);
    CGPoint getScreenXY() override;
    virtual void calcFrame() { setupTexCoords(); }

    // FlixelAPTBindings (FlxSprite category)
    FlxSprite* loadGraphicWithParam1(const std::string& Graphic, BOOL Animated = false, BOOL Reverse = false, unsigned int Width = 0,
                                     unsigned int Height = 0, BOOL Unique = false) {
        return loadGraphic(Graphic, Animated, Reverse, Width, Height, Unique);
    }
    void addAnimationWithParam1(const std::string& Name, const std::vector<unsigned int>& Frames, float FrameRate = 0, BOOL Looped = true) {
        addAnimation(Name, Frames, FrameRate, Looped);
    }
    FlxSprite* loadRotatedGraphicWithParam1(const std::string& Graphic, unsigned int Rotations, int Frame = -1, BOOL AntiAliasing = false,
                                            BOOL AutoBuffer = false) {
        return loadRotatedGraphic(Graphic, Rotations, Frame, AntiAliasing, AutoBuffer);
    }
};

// PORT: FlxManagedSprite pooled ObjC instances through a custom alloc/retain/release; here it is a
// plain FlxSprite whose init re-initialises every field the way -reInitWithX:... did.
class FlxManagedSprite : public FlxSprite {
public:
    static void initializeWithManagedPoolSize(unsigned int) {}
    FlxSprite* initWithX(float X, float Y, const std::string& SimpleGraphic, float ModelScale = 1.0f) override;
    using FlxSprite::initWithX;
};

class FlxText : public FlxSprite {
public:
    ~FlxText() override;  // PORT: frees its private text texture and SemiSecretText
    SemiSecretText* sstext = nullptr;
    BOOL _regen = false;
    unsigned int _shadow = 0;
    BOOL regen = false;

    float getHeight() override;
    // + (id) textWithWidth:(unsigned int)Width text:(NSString *)Text font:(NSString *)Font size:(float)Size modelScale:(float)ModelScale;
    static FlxText* textWithWidth(unsigned int Width, const std::string& Text, const std::string& Font = std::string(), float Size = 0,
                                  float ModelScale = 1.0f);
    // - (id) initWithX:(float)X y:(float)Y width:(unsigned int)Width text:(NSString *)Text modelScale:(float)ModelScale;
    FlxText* initWithX(float X, float Y, unsigned int Width, const std::string& Text = std::string(), float ModelScale = 1.0f);
    // - (FlxText *) setFormatWithParam1:param2:param3:param4:param5:
    FlxText* setFormatWithParam1(const std::string& Font = std::string(), float Size = 8, unsigned int Color = 0xffffff,
                                 const std::string& Alignment = std::string(), unsigned int ShadowColor = 0);
    std::string text();
    void setText(const std::string& Text);
    float size();
    void setSize(float Size);
    unsigned int color();
    void setColor(unsigned int Color) override;
    std::string font();
    void setFont(const std::string& Font);
    std::string alignment();
    void setAlignment(const std::string& Alignment);
    unsigned int shadow();
    void setShadow(unsigned int ShadowColor);
    void setupNewText();
    void render() override;
    void sizeToFit();
};

class FlxEmitter : public FlxGroup {
public:
    CGPoint minParticleSpeed, maxParticleSpeed, particleDrag;
    float minRotation = 0, maxRotation = 0, gravity = 0;
    BOOL on = false;
    float delay = 0;
    unsigned int quantity = 0;
    BOOL _explode = true;
    float _timer = 0;
    unsigned int _particle = 0;
    unsigned int _counter = 0;

    FlxEmitter* init() override { return initWithEmitterX(0, 0); }
    // - (id) initWithX:(float)X y:(float)Y;
    FlxEmitter* initWithEmitterX(float X, float Y);
    // - (FlxEmitter *) createSprites:quantity:bakedRotations:multiple:collide:modelScale:
    FlxEmitter* createSprites(const std::string& Graphics, unsigned int Quantity = 50, unsigned int BakedRotations = 16,
                              BOOL Multiple = true, float Collide = 0, float ModelScale = 1.0f);
    void setSizeWithParam1(unsigned int Width, unsigned int Height) { width = Width; height = Height; }
    void setXSpeedWithParam1(float Min = 0, float Max = 0) { minParticleSpeed.x = Min; maxParticleSpeed.x = Max; }
    void setYSpeedWithParam1(float Min = 0, float Max = 0) { minParticleSpeed.y = Min; maxParticleSpeed.y = Max; }
    void setRotationWithParam1(float Min = 0, float Max = 0) { minRotation = Min; maxRotation = Max; }
    virtual void updateEmitter();
    void updateMembers() override;
    void update() override;
    // - (void) startWithParam1:(BOOL)Explode param2:(float)Delay param3:(unsigned int)Quantity;
    void start(BOOL Explode = true, float Delay = 0, unsigned int Quantity = 0);
    virtual void emitParticle();
    // - (void) stop:(float)Delay;
    void stop(float Delay = 3);
    void at(FlxObject* Object);
    void kill() override;
    // FlixelAPTBindings (FlxEmitter category)
    FlxEmitter* createSpritesWithParam1(const std::string& Graphics, unsigned int Quantity, unsigned int BakedRotations = 16,
                                        BOOL Multiple = true, float Collide = 0) {
        return createSprites(Graphics, Quantity, BakedRotations, Multiple, Collide);
    }
};

class FlxButton : public FlxGroup {
public:
    BOOL _onToggle = false;
    FlxSprite* _off = nullptr;
    FlxSprite* _on = nullptr;
    FlxText* _offT = nullptr;
    FlxText* _onT = nullptr;
    FlashFunction* _callback = nullptr;
    BOOL _pressed = false, _initialized = false;
    FlxPoint* _sf = nullptr;

    // - (id) initWithX:(int)X y:(int)Y callback:(FlashFunction *)Callback;
    FlxButton* initWithX(int X, int Y, FlashFunction* Callback);
    using FlxGroup::initWithX;
    // - (FlxButton *) loadGraphicWithParam1:(FlxSprite *)Image param2:(FlxSprite *)ImageHighlight;
    FlxButton* loadGraphicWithParam1(FlxSprite* Image, FlxSprite* ImageHighlight = nullptr);
    // - (FlxButton *) loadTextWithParam1:(FlxText *)Text param2:(FlxText *)TextHighlight;
    FlxButton* loadTextWithParam1(FlxText* Text, FlxText* TextHighlight = nullptr);
    void update() override;
    BOOL on() { return _onToggle; }
    void setOn(BOOL On) { _onToggle = On; }
    void destroy() override {}
    void visibility(BOOL On);
};

class FlxFade : public FlxSprite {
public:
    float _delay = 0;
    FlashFunction* _complete = nullptr;
    FlxFade* init() override;
    // - (void) startWithParam1:(unsigned int)Color param2:(float)Duration param3:(FlashFunction *)FadeComplete param4:(BOOL)Force;
    void start(unsigned int Color = 0xff000000, float Duration = 1, FlashFunction* FadeComplete = nullptr, BOOL Force = false);
    void stop() { exists = false; }
    void update() override;
};

class FlxFlash : public FlxSprite {
public:
    float _delay = 0;
    FlashFunction* _complete = nullptr;
    FlxFlash* init() override;
    void start(unsigned int Color = 0xffffffff, float Duration = 1, FlashFunction* FlashComplete = nullptr, BOOL Force = false);
    void stop() { exists = false; visible = false; }
    void update() override;
};

class FlxQuake {
public:
    float _zoom = 1, _intensity = 0, _timer = 0;
    int x = 0, y = 0;
    CGPoint scale;
    static float intensityScale;  // PORT enhancement: screen-shake multiplier (1 = original)
    // - (id) initWithZoom:(float)Zoom;
    FlxQuake* initWithZoom(float Zoom);
    // - (void) startWithIntensity:(float)Intensity duration:(float)Duration;
    void start(float Intensity = 0.05f, float Duration = 0.5f);
    void startWithParam1(float Intensity, float Duration) { start(Intensity, Duration); }  // APT binding
    void stop();
    void update();
};

class FlxState {
public:
    FlxGroup* defaultGroup = nullptr;
    unsigned int bgColor = 0;
    float red = 0, green = 0, blue = 0, alpha = 0;
    FlxZone zone;  // PORT: owns every Flixel object allocated for this state (see FlxZone)
    virtual ~FlxState() {}
    // - (id) init;
    virtual FlxState* init();
    virtual void create() {}
    virtual FlxObject* add(FlxObject* Core) { return defaultGroup->add(Core); }
    virtual void remove(FlxObject* Core) { defaultGroup->remove(Core); }
    virtual void preProcess();
    virtual void update() { defaultGroup->update(); }
    virtual void collide() { defaultGroup->collide(); }
    virtual void render() { defaultGroup->render(); }
    virtual void postProcess() {}
    virtual void destroy() { defaultGroup->destroy(); }
    virtual void setPaused(BOOL) {}
    void setBgColor(unsigned int BGColor);
};

class FlxTouches {
public:
    BOOL newData = false;
    int touchCount = 0;  // PORT: NSSet *touches -> count + the "anyObject" location
    CGPoint anyTouchLocation;
    BOOL touchesBegan = false, touchesEnded = false, touching = false;
    BOOL nextTouchesBegan = false, nextTouchesEnded = false;
    CGPoint nextScreenTouchPoint, nextLastScreenTouchPoint, screenTouchPoint, lastScreenTouchPoint;
    CGPoint nextScreenTouchBeganPoint, screenTouchBeganPoint;

    // - (void) processTouches:(NSSet *)newTouches   (count + location of [touches anyObject])
    void processTouches(int newCount, CGPoint location);
    void update();
    CGPoint touchPoint();
    CGPoint lastTouchPoint();
    CGPoint touchBeganPoint();
    CGPoint internalScreenTouchPoint();
};

class FlxSave {
public:
    std::string name;
    bool bound = false;
    // - (BOOL) bind:(NSString *)Name;
    BOOL bind(const std::string& Name);
    // - (BOOL) writeWithParam1:(NSString *)FieldName param2:(id)FieldValue param3:(unsigned int)MinFileSize;
    BOOL writeWithParam1(const std::string& FieldName, const std::string& FieldValue, unsigned int MinFileSize = 0);
    std::string read(const std::string& FieldName);
    BOOL forceSave(unsigned int MinFileSize = 0);
    BOOL erase(unsigned int MinFileSize = 0);
};

// FlxSound is a stub in flixel-ios (all OpenAL code commented out); FlxG does the real audio.
class FlxSound : public FlxObject {
public:
    float _volume = 1;
    FlxSound* init() { initWithX(0, 0, 0, 0); return this; }
    FlxSound* loadEmbeddedWithParam1(const std::string&, BOOL = false) { return this; }
    FlxSound* loadStreamWithParam1(const std::string&, BOOL = false) { return this; }
    void play() {}
    void pause() {}
    void stop() {}
    void fadeOutWithParam1(float, BOOL = false) {}
    void fadeIn(float) {}
    float volume() { return _volume; }
    void setVolume(float v) { _volume = v; }
};

class FlxList {
public:
    FlxObject* object = nullptr;
    FlxList* next = nullptr;
};

class FlxQuadTree : public FlxRect {
public:
    BOOL _canSubdivide = false;
    FlxList *_headA = nullptr, *_tailA = nullptr, *_headB = nullptr, *_tailB = nullptr;
    FlxQuadTree *_nw = nullptr, *_ne = nullptr, *_se = nullptr, *_sw = nullptr;
    float _l = 0, _r = 0, _t = 0, _b = 0, _hw = 0, _hh = 0, _mx = 0, _my = 0;
    std::function<BOOL(FlxObject*, FlxObject*)> target;  // PORT: target/action pair

    static float MIN() { return 160; }
    static unsigned int A_LIST() { return 0; }
    static unsigned int B_LIST() { return 1; }
    ~FlxQuadTree() override;
    // - (id) initWithX:(float)X y:(float)Y width:(float)Width height:(float)Height parent:(FlxQuadTree *)Parent;
    FlxQuadTree* initWithX(float X, float Y, float Width, float Height, FlxQuadTree* Parent = nullptr);
    void addWithParam1(FlxObject* Object, unsigned int List);
    void addObject();
    void addToList();
    // - (BOOL) overlapWithParam1:(BOOL)BothLists target:(id)target action:(SEL)action;
    BOOL overlapWithParam1(BOOL BothLists = true, std::function<BOOL(FlxObject*, FlxObject*)> Target = nullptr);
    BOOL overlapNode(FlxList* Iterator = nullptr);
};

class FlxTileblock : public FlxObject {
public:
    SemiSecretTexture* texture = nullptr;
    std::vector<GLshort> verticesUVs;
    GLshort uShort = 0, vShort = 0;
    float tileSize = 0;
    unsigned int vertexCount = 0, empties = 0;
    static FlxTileblock* tileblockWithX(float X, float Y, float Width, float Height);
    FlxTileblock* initWithX(float X, float Y, float Width, float Height) override;
    // - (id) loadGraphic:(NSString *)TileGraphic empties:(unsigned int)Empties;
    FlxTileblock* loadGraphic(const std::string& TileGraphic, unsigned int Empties = 0);
    void render() override;
    void renderBlock();
};

// ---------------------------------------------------------------------------------------------
// FlxU / FlxG / FlxGame / FlxGLView statics
// ---------------------------------------------------------------------------------------------
class FlxU {
public:
    static void setSeed(float s);
    static float seed();
    static float roundingError();
    static void openURL(const std::string& URL);
    static float abs(float N) { return (N > 0) ? N : -N; }
    static float floor(float N);
    static float ceil(float N);
    // + (float) random:(BOOL)UseGlobalSeed;
    static float random(BOOL UseGlobalSeed = true);
    static float randomize(float Seed);
    static float mutateWithParam1(float Seed, float Mutator);
    static CGPoint rotatePointWithParam1(float X, float Y, float PivotX, float PivotY, float Angle);
    static float getAngleWithParam1(float X, float Y);
    // + (float) computeVelocityWithParam1:(float)Velocity param2:(float)Acceleration param3:(float)Drag param4:(float)Max;
    static float computeVelocityWithParam1(float Velocity, float Acceleration = 0, float Drag = 0, float Max = 10000);
    static void setWorldBoundsWithParam1(float X = 0, float Y = 0, float Width = 0, float Height = 0);
    static BOOL overlapWithParam1(FlxObject* Object1, FlxObject* Object2);
    static BOOL collideWithParam1(FlxObject* Object1, FlxObject* Object2);
    static BOOL alternateCollideWithParam1(FlxObject* Object1, FlxObject* Object2);
    static BOOL collideObject(FlxObject* object, FlxGroup* group);
    static BOOL solveCollisionWithParam1(FlxObject* Object1, FlxObject* Object2);
    static BOOL solveXCollisionWithParam1(FlxObject* Object1, FlxObject* Object2);
    static BOOL solveYCollisionWithParam1(FlxObject* Object1, FlxObject* Object2);
};

class FlxGLView {
public:
    static void setTextureScale(float s);
    static float textureScale();
    static GLshort convertToShort(float value);
    static float convertFromShort(GLshort value);
    static int maxTextureSize() { return 1024; }
};

// Device being emulated (FlxG +initialize read these from UIDevice/UIScreen).
struct FlxDevice {
    int pointsWidth = 320, pointsHeight = 480;  // portrait UIScreen bounds
    bool iPad = false, retina = false;
    std::string platform = "iPhone3,1";
    int windowScale = 2;
    std::string assetDir = "assets";  // port-only: where the extracted IPA assets live
};

class FlxG {
public:
    static FlxDevice device;
    static void setMusic(FlxSound* m);
    static FlxSound* music();
    static void setQuake(FlxQuake* q);
    static FlxQuake* quake();
    static void setLevel(int l);
    static int level();
    static void setState(FlxState* State);
    static FlxState* state();
    static void setElapsed(float e);
    static float elapsed();
    static void setMaxElapsed(float e);
    static float maxElapsed();
    static void setScroll(FlashPoint* s);
    static FlashPoint* scroll();
    static void setScore(int s);
    static int score();
    static void setFlash(FlxFlash* f);
    static FlxFlash* flash();
    static void setFade(FlxFade* f);
    static FlxFade* fade();
    static void setHeight(int h);
    static int height();
    static void setWidth(int w);
    static int width();
    static void setTimeScale(float t);
    static float timeScale();
    static FlxTouches* touches();
    static FlxGame* game();
    static void setPauseFollow(BOOL p);
    static BOOL pauseFollow();
    static float screenScale();
    static BOOL iPad();
    static BOOL retinaDisplay();
    static BOOL iPhone1G();
    static BOOL iPhone3G();
    static BOOL iPhone3GS();
    static BOOL iPodTouch1G();
    static BOOL iPodTouch2G();
    static BOOL iPodTouch3G();
    static void initialize();
    static void log(const std::string& Data);
    // + (void) playMusicWithParam1:(NSString *)Music param2:(float)Volume;
    static void playMusic(const std::string& Music, float Volume = 1);
    static void reallyPlayMusic(const std::string& Music);
    static BOOL isOtherMusicPlaying() { return false; }
    static void fadeOutMusic(float duration);
    static void pauseMusic();
    static void unpauseMusic();
    static void loadSound(const std::string& filename);
    // + (FlxSound *) playWithParam1:(NSString *)EmbeddedSound param2:(float)Volume param3:(BOOL)Looped;
    static FlxSound* play(const std::string& EmbeddedSound, float Volume = 1, BOOL Looped = false);
    static BOOL playing(const std::string& EmbeddedSound);
    static void stop(const std::string& EmbeddedSound);
    static void vibrate();
    static BOOL checkBitmapCache(const std::string& Key);
    static SemiSecretTexture* addTextureWithParam1(const std::string& Graphic, BOOL Unique);
    static SemiSecretTexture* createBitmapWithParam1(unsigned int Width, unsigned int Height, unsigned int Color, BOOL Unique, const std::string& Key);
    static void clearTextureCache();
    static void setTexture(SemiSecretTexture* Texture, const std::string& Key);
    static void removeTexture(const std::string& Key);  // PORT: frees a zone's unique texture
    static void updateSounds();
    static void followWithParam1(FlxObject* Target, float Lerp = 1);
    static void followAdjustWithParam1(float LeadX = 0, float LeadY = 0);
    static void followBoundsWithParam1(int MinX = 0, int MinY = 0, int MaxX = 0, int MaxY = 0, BOOL UpdateWorldBounds = true);
    static void setGameData(FlxGame* Game, int Width, int Height, float Zoom);
    static void doFollow();
    static void unfollow();
    static void didEnterBackground();
    static void willEnterForeground();
    static void willResignActive();
    static void didBecomeActive();
    static float soundEffectsMasterVolume;
    static float musicMasterVolume;
};

enum FlxGameOrientation { FlxGameOrientationPortrait, FlxGameOrientationLandscape };

class FlxGame {
public:
    std::function<FlxState*()> _iState;  // PORT: NSString class name -> factory
    BOOL _created = false;
    FlxState* _state = nullptr;
    float _zoom = 2;
    float modelZoom = 1;
    BOOL textureBufferZoom = false;
    int _gameXOffset = 0, _gameYOffset = 0;
    float _elapsed = 0;
    unsigned int _total = 0;
    BOOL _paused = false;
    BOOL autorotate = true;
    FlxGameOrientation gameOrientation = FlxGameOrientationLandscape;
    int backingWidth = 0, backingHeight = 0;  // portrait renderbuffer size, as on the device
    BOOL iPad = false;
    FlxState* newState = nullptr;
    int frameInterval = 2;
    // PORT: switched-out states, freed a few frames later (the original autoreleased them; deferred
    // performSelector:afterDelay: calls kept their targets alive until they fired).
    struct DeadState { FlxState* state; unsigned int frame; };
    std::vector<DeadState> _graveyard;
    unsigned int _frameCount = 0;
    void collectGraveyard(bool force = false);
    // port-only test hooks
    int shotFrame = -1;
    double previewSeconds = 0;  // port-only: exit after N seconds (launcher "Apply & test")
    std::string shotFile;

    virtual ~FlxGame() {}
    // - (id) initWithOrientation:state:zoom:useTextureBufferZoom:modelZoom:
    virtual FlxGame* initWithOrientation(FlxGameOrientation GameOrientation, std::function<FlxState*()> InitialState, float Zoom = 2.0f,
                                         BOOL TextureBufferZoom = false, float ModelZoom = 1.0f);
    BOOL paused() { return _paused; }
    void setPaused(BOOL p);
    float zoom() { return _zoom; }
    static void setSlowdown(unsigned int s);
    static unsigned int slowdown();
    void start();
    void resetProjection();
    void showSoundTray(BOOL Silent = false) {}
    void switchState(FlxState* State);
    void update();
    void didEnterBackground() {}
    void willEnterForeground() {}
    // PORT: replaces CADisplayLink + UIApplicationMain run loop. Returns when the window closes.
    int run();
};

// ---------------------------------------------------------------------------------------------
// SemiSecret
// ---------------------------------------------------------------------------------------------
enum SSTextureFiltering { SSTextureFilteringDefault, SSTextureFilteringNearest, SSTextureFilteringLinear };
enum SSTextureDepth { SSTextureDepth32Bits, SSTextureDepth16Bits5551, SSTextureDepth16Bits4444 };

class SemiSecretTexture {
public:
    plat::Texture* texture = nullptr;  // GLuint texture
    CGSize size, paddedSize;
    CGPoint offset;
    SemiSecretTexture* atlasTexture = nullptr;
    CGPoint atlasOffset, atlasScale;
    GLshort vertexData[4 * 2] = {};
    std::vector<GLshort> texCoords;
    unsigned int frames = 0;
    BOOL animated = false;
    std::vector<GLubyte> data;

    static void setTextureFilteringMode(SSTextureFiltering f);
    static SSTextureFiltering textureFilteringMode();
    static void setTextureDepth(SSTextureDepth d);
    static SSTextureDepth textureDepth();
    static SemiSecretTexture* textureWithImage(const std::string& filename);
    static SemiSecretTexture* textureWithColor(unsigned int color);
    static SemiSecretTexture* textureWithView(SemiSecretText* view);
    SemiSecretTexture* initWithImage(const std::string& filename);
    SemiSecretTexture* initWithView(SemiSecretText* view);
    SemiSecretTexture* initWithColor(unsigned int color);
    ~SemiSecretTexture();
    unsigned int width() { return (unsigned int)size.width; }
    unsigned int height() { return (unsigned int)size.height; }
    unsigned int paddedWidth() { return (unsigned int)paddedSize.width; }
    unsigned int paddedHeight() { return (unsigned int)paddedSize.height; }
    void prepareWithWidth(int width, int height);
    void setAnimated(BOOL Animated);
    void setupVerticesTexCoords();
    void createTexture();
};

// Font metrics are read from the TrueType file (stb_truetype) instead of CGFont.
class SemiSecretFont {
public:
    float size = 0;
    struct FontData* font = nullptr;  // CGFontRef
    virtual ~SemiSecretFont() {}
    // + (SemiSecretFont *)fontWithName:(NSString *)name size:(CGFloat)size;
    static SemiSecretFont* fontWithName(const std::string& name, float size);
    virtual SemiSecretFont* fontWithSize(float s);
    virtual SemiSecretFont* initWithSize(float fontsize) { size = fontsize; font = nullptr; return this; }
    virtual void setGlyphs(uint16_t* glyphs, const unichar* chars, size_t length);
    CGSize sizeToRenderString(const std::u16string& string);
    virtual std::string className() { return "SemiSecretFont"; }
};

class FlixelFont : public SemiSecretFont {
public:
    SemiSecretFont* initWithSize(float fontsize) override;
    SemiSecretFont* fontWithSize(float s) override;
    void setGlyphs(uint16_t* glyphs, const unichar* chars, size_t length) override;
    std::string className() override { return "FlixelFont"; }
};

enum SemiSecretTextEffect {
    SemiSecretTextEffectNone,
    SemiSecretTextEffectEngravedShadow,
    SemiSecretTextEffectReverseEngravedShadow,
    SemiSecretTextEffectBlurredShadow,
};
enum SemiSecretTextAlign { SemiSecretTextAlignCenter, SemiSecretTextAlignLeft, SemiSecretTextAlignRight };

struct CGColor { float r = 1, g = 1, b = 1, a = 1; };

// PORT: SemiSecretText was a UIView rendered with -drawRect: into a CGBitmapContext. Here it keeps
// the same layout state and rasterises glyphs itself (drawRect -> renderInto).
class SemiSecretText {
public:
    std::u16string text;
    BOOL useSSFont = false;
    SemiSecretFont* ssfont = nullptr;
    CGColor color{1, 1, 1, 1};
    CGColor shadowColor{0, 0, 0, 1};
    std::vector<uint16_t> glyphs;
    float lineWidth = 0, textWidth = 0, textHeight = 0;
    SemiSecretTextEffect effect = SemiSecretTextEffectNone;
    SemiSecretTextAlign align = SemiSecretTextAlignCenter;
    BOOL wrapped = false;
    float lineSpacing = 1.6f;
    BOOL reposition = true;
    float padding = 5.0f;
    BOOL wrapOnSpace = true;
    BOOL debug = false;
    CGRect frame;   // UIView frame (origin + size); bounds.size == frame.size here
    CGRect bounds;

    SemiSecretText* initWithFrame(CGRect f);
    void setPadding(float p);
    void setText(const std::u16string& t);
    void setFont(SemiSecretFont* f);
    SemiSecretFont* font() { return useSSFont ? ssfont : nullptr; }
    void setFrame(CGRect f) { frame = f; bounds = CGRectMake(0, 0, f.size.width, f.size.height); }
    float heightForLines(int lines, float width);
    long nextWrapOffsetForGlyphs(long offset);
    void computeNewBounds();
    // - (void) drawRect:(CGRect)rect  ->  premultiplied RGBA buffer, top-left origin, stride = pw*4
    void renderInto(uint8_t* rgba, int pw, int ph);
};

std::u16string toU16(const std::string& utf8);
std::string toU8(const std::u16string& s);
