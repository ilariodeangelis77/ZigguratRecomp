//
//  FlxSprite.cpp — FlxSprite.m, FlxManagedSprite.m, FlxAnim.m, FlxFade.m, FlxFlash.m, FlxQuake.m, FlxTileblock.m
//
//  Copyright Semi Secret Software 2009-2010. All rights reserved.
//  C++ port: MIT license (see ENGINE_LICENSE.txt).
//
#include <SDL3/SDL.h>

#include "Flixel.h"

const unsigned int LEFT = 0;
const unsigned int RIGHT = 1;
const unsigned int UP = 2;
const unsigned int DOWN = 3;

// ============================== FlxAnim ==============================
FlxAnim* FlxAnim::initWithParam1(const std::string& Name, const std::vector<unsigned int>& Frames, float FrameRate, BOOL Looped) {
    name = Name;
    delay = 0;
    if (FrameRate > 0) delay = 1 / FrameRate;
    frames = Frames;
    looped = Looped;
    return this;
}

// ============================== FlxSprite ==============================

// PORT: -dealloc released _animations; textures are shared via the FlxG cache (not owned here).
FlxSprite::~FlxSprite() {
    for (FlxAnim* a : _animations) delete a;
}

// + (id) spriteWithX:(float)X y:(float)Y graphic:(NSString *)SimpleGraphic;
FlxSprite* FlxSprite::spriteWithX(float X, float Y, const std::string& SimpleGraphic) {
    return (new FlxSprite)->initWithX(X, Y, SimpleGraphic, 1.0f);
}

// - (id) initWithX:(float)X y:(float)Y graphic:(NSString *)SimpleGraphic modelScale:(float)ModelScale;
FlxSprite* FlxSprite::initWithX(float X, float Y, const std::string& SimpleGraphic, float ModelScale) {
    FlxObject::initWithX(X, Y, 0, 0);
    red = 255;
    green = 255;
    blue = 255;
    for (int i = 0; i < 4 * 4; ++i) _colors[i] = 255;
    filled = false;
    x = X;
    y = Y;
    modelScale = ModelScale;
    if (SimpleGraphic.empty())
        createGraphicWithWidth(8, 8);
    else
        loadGraphicModelScale(SimpleGraphic, modelScale);
    offset = CGPointZero;
    scale = CGPointMake(1, 1);
    _alpha = 1;
    _color = 0x00ffffff;
    blend.clear();
    antialiasing = false;
    finished = false;
    _facing = RIGHT;
    _animations.clear();
    _flipped = 0;
    _curAnim = nullptr;
    _curFrame = 0;
    _caf = 0;
    _frameTimer = 0;
    _callback = nullptr;
    setupVertices();
    setupTexCoords();
    return this;
}

// - (FlxSprite *) loadGraphic:animated:reverse:width:height:unique:modelScale:
FlxSprite* FlxSprite::loadGraphic(const std::string& Graphic, BOOL Animated, BOOL Reverse, unsigned int Width, unsigned int Height,
                                  BOOL Unique, float ModelScale) {
    modelScale = ModelScale;
    fill(0xffffff);
    filled = false;
    _bakedRotation = 0;
    texture = FlxG::addTextureWithParam1(Graphic, Unique);
    CGSize tsize = texture ? texture->size : CGSize{};  // messaging nil returns a zero struct
    if (Reverse)
        _flipped = ((int)(tsize.width)) >> 1;
    else
        _flipped = 0;
    if (Width == 0) {
        if (Animated)
            Width = tsize.height / modelScale;
        else
            Width = tsize.width / modelScale;
    }
    width = frameWidth = Width;
    if (Height == 0) {
        if (Animated)
            Height = width;
        else
            Height = tsize.height / modelScale;
    }
    height = frameHeight = Height;
    if (Animated && texture) texture->setAnimated(true);
    resetHelpers();
    return this;
}

// - (FlxSprite *) loadRotatedGraphic:rotations:frame:antiAliasing:autoBuffer:modelScale:
FlxSprite* FlxSprite::loadRotatedGraphic(const std::string& Graphic, unsigned int Rotations, int Frame, BOOL AntiAliasing, BOOL AutoBuffer,
                                         float ModelScale) {
    unsigned int rows = 4;
    modelScale = ModelScale;
    FlxSprite* brush = (new FlxSprite)->init()->loadGraphic(Graphic, (Frame >= 0), false, 0, 0, false, ModelScale);
    if (Frame >= 0) brush->setFrame(Frame);
    unsigned int max = brush->getWidth();
    if (brush->getHeight() > max) max = brush->getHeight();
    if (AutoBuffer) max *= 1.5;
    unsigned int cols = ceil(Rotations / rows);
    width = max * cols;
    height = max * rows;
    char keybuf[512];
    snprintf(keybuf, sizeof keybuf, "%s:%d:%fx%f", Graphic.c_str(), Frame, width, height);
    std::string key = keybuf;
    BOOL skipGen = FlxG::checkBitmapCache(key);
    createGraphicWithWidth(width, height, 0, true, key);
    _bakedRotation = 360 / Rotations;
    if (!skipGen) {
        unsigned int bw2 = brush->getWidth() / 2;
        unsigned int bh2 = brush->getHeight() / 2;
        unsigned int gxc = max / 2;
        unsigned int gyc = max / 2;
        for (unsigned int r = 0; r < rows; r++) {
            for (unsigned int c = 0; c < cols; c++) {
                drawWithParam1(brush, gxc + max * c - bw2, gyc - bh2);
                brush->angle += _bakedRotation;
            }
            gyc += max;
        }
    }
    frameWidth = frameHeight = width = height = max;
    resetHelpers();
    return this;
}

// - (FlxSprite *) createGraphicWithWidth:height:color:unique:key:
FlxSprite* FlxSprite::createGraphicWithWidth(unsigned int Width, unsigned int Height, unsigned int Color, BOOL Unique, const std::string& Key) {
    fill(Color);
    width = frameWidth = Width;
    height = frameHeight = Height;
    resetHelpers();
    return this;
}

void FlxSprite::setWidth(float Width) {
    FlxRect::setWidth(Width);
    resetHelpers();
}
void FlxSprite::setHeight(float Height) {
    FlxRect::setHeight(Height);
    resetHelpers();
}
void FlxSprite::setFrameHeight(float h) {
    frameHeight = h;
    resetHelpers();
}
void FlxSprite::setFrameWidth(float w) {
    frameWidth = w;
    resetHelpers();
}

// - (void) setupTexCoords
// PORT: texture atlases are never built by this port, so only the non-atlas branch is needed.
void FlxSprite::setupTexCoords() {
    if (texture == nullptr) return;
    int xframes = texture->size.width / frameWidth / modelScale;
    if (xframes == 0) xframes = 1;  // PORT: guard against div-by-zero (ARM __modsi3 path undefined)
    _uOffset = _caf % xframes;
    _vOffset = _caf / xframes;
    _uShort = FlxGLView::convertToShort(frameWidth * modelScale / texture->paddedSize.width);
    _vShort = FlxGLView::convertToShort(frameHeight * modelScale / texture->paddedSize.height);
    _verticesUVs[2] = _uOffset * _uShort;
    _verticesUVs[3] = _vOffset * _vShort;
    _verticesUVs[6] = (_uOffset + 1) * _uShort;
    _verticesUVs[7] = _vOffset * _vShort;
    _verticesUVs[10] = _uOffset * _uShort;
    _verticesUVs[11] = (_vOffset + 1) * _vShort;
    _verticesUVs[14] = (_uOffset + 1) * _uShort;
    _verticesUVs[15] = (_vOffset + 1) * _vShort;
}

// - (void) setupVertices
void FlxSprite::setupVertices() {
    if (_flipped) {
        _verticesUVs[0] = (GLshort)frameWidth;
        _verticesUVs[1] = 0;
        _verticesUVs[4] = 0;
        _verticesUVs[5] = 0;
        _verticesUVs[8] = (GLshort)frameWidth;
        _verticesUVs[9] = (GLshort)frameHeight;
        _verticesUVs[12] = 0;
        _verticesUVs[13] = (GLshort)frameHeight;
    } else {
        _verticesUVs[0] = 0;
        _verticesUVs[1] = 0;
        _verticesUVs[4] = (GLshort)frameWidth;
        _verticesUVs[5] = 0;
        _verticesUVs[8] = 0;
        _verticesUVs[9] = (GLshort)frameHeight;
        _verticesUVs[12] = (GLshort)frameWidth;
        _verticesUVs[13] = (GLshort)frameHeight;
    }
}

// - (void) resetHelpers;
void FlxSprite::resetHelpers() {
    setupVertices();
    setupTexCoords();
    origin.x = frameWidth / 2;
    origin.y = frameHeight / 2;
    _caf = 0;
    refreshHulls();
}

// - (void) setFacing:(unsigned int)Direction;
void FlxSprite::setFacing(unsigned int Direction) {
    BOOL c = _facing != Direction;
    _facing = Direction;
    if (c) calcFrame();
}

// - (void) setAlpha:(float)Alpha;   (sic: blue/green swapped in the colour array, as in the original)
void FlxSprite::setAlpha(float Alpha) {
    if (Alpha > 1) Alpha = 1;
    if (Alpha < 0) Alpha = 0;
    if (Alpha == _alpha) return;
    _alpha = Alpha;
    _colors[3] = _colors[7] = _colors[11] = _colors[15] = (GLubyte)(_alpha * 255);
    _colors[0] = _colors[4] = _colors[8] = _colors[12] = (GLubyte)(_alpha * red);
    _colors[1] = _colors[5] = _colors[9] = _colors[13] = (GLubyte)(_alpha * blue);
    _colors[2] = _colors[6] = _colors[10] = _colors[14] = (GLubyte)(_alpha * green);
    calcFrame();
}

// - (void) setColor:(unsigned int)Color;  (not implemented in flixel-ios)
void FlxSprite::setColor(unsigned int Color) { SDL_Log("setColor called!"); }

// - (void) drawWithParam1:(FlxSprite *)Brush param2:(int)X param3:(int)Y;
// PORT: the original rendered the brush into the *current framebuffer* (not into this sprite, whose
// texture is nil after -createGraphic), so nothing persisted. We keep the bookkeeping, skip the draw.
void FlxSprite::drawWithParam1(FlxSprite* Brush, int X, int Y) {
    int bx = Brush->x;
    int by = Brush->y;
    Brush->setX(X + bx);
    Brush->setY(Y + by);
    Brush->setX(bx);
    Brush->setY(by);
}

// - (void) fill:(unsigned int)Color;
void FlxSprite::fill(unsigned int Color) {
    _bakedRotation = 0;
    texture = nullptr;
    red = (Color >> 16) & 0xff;
    green = (Color >> 8) & 0xff;
    blue = (Color >> 0) & 0xff;
    for (int i = 0; i < 4; ++i) {
        _colors[i * 4] = red * _alpha;
        _colors[i * 4 + 1] = green * _alpha;
        _colors[i * 4 + 2] = blue * _alpha;
        _colors[i * 4 + 3] = _alpha * 255;
    }
    filled = true;
}

// - (void) updateAnimation;
void FlxSprite::updateAnimation() {
    if (_bakedRotation) {
        unsigned int oc = _caf;
        double v = fmod(angle, 360) / _bakedRotation;
        _caf = v <= 0 ? 0 : (unsigned int)v;  // ARM VCVT to unsigned saturates negatives to 0
        if (oc != _caf) calcFrame();
        return;
    }
    if ((_curAnim != nullptr) && (_curAnim->delay > 0) && (_curAnim->looped || !finished)) {
        _frameTimer += FlxG::elapsed();
        while (_frameTimer > _curAnim->delay) {
            _frameTimer -= _curAnim->delay;
            if (_curFrame == _curAnim->frames.size() - 1) {
                if (_curAnim->looped) _curFrame = 0;
                finished = true;
            } else
                _curFrame++;
            _caf = _curAnim->frames[_curFrame];
            calcFrame();
        }
    }
}

// - (void) update;
void FlxSprite::update() {
    updateMotion();
    updateAnimation();
    updateFlickering();
}

// - (void) renderSprite;
void FlxSprite::renderSprite() {
    if (!visible) return;
    plat::Blend blendMode = enableBlend ? plat::Blend::Normal : plat::Blend::None;
    CGPoint _point = getScreenXY();
    if (!blend.empty() && blend == "screen") blendMode = plat::Blend::Screen;
    plat::glPushMatrix();
    plat::glTranslatef(_point.x, _point.y);
    if (rotationOrigin.x == 0 || rotationOrigin.y == 0) {
        if (scale.x != 1 || scale.y != 1 || angle != 0 || _facing == LEFT) {
            plat::glTranslatef(origin.x, origin.y);
            if (scale.x != 1 || scale.y != 1) plat::glScalef(scale.x, scale.y);
            if (angle != 0) plat::glRotatef(angle);
            if (_facing == LEFT) plat::glScalef(-1.0, 1.0);
            plat::glTranslatef(-origin.x, -origin.y);
        }
    } else {
        // ZiGGURAT engine (binary 0x17820): scale/flip about origin, rotate about rotationOrigin.
        if (scale.x != 1 || scale.y != 1 || _facing == LEFT) {
            plat::glTranslatef(origin.x, origin.y);
            if (scale.x != 1 || scale.y != 1) plat::glScalef(scale.x, scale.y);
            if (_facing == LEFT) plat::glScalef(-1.0, 1.0);
            plat::glTranslatef(-origin.x, -origin.y);
        }
        if (angle != 0) {
            plat::glTranslatef(rotationOrigin.x, rotationOrigin.y);
            plat::glRotatef(angle);
            plat::glTranslatef(-rotationOrigin.x, -rotationOrigin.y);
        }
    }
    const GLubyte* colors = (_alpha != 1.0 || filled) ? _colors : nullptr;
    float xy[8], uv[8];
    const float ts = FlxGLView::textureScale();
    for (int i = 0; i < 4; i++) {
        xy[i * 2] = _verticesUVs[i * 4];
        xy[i * 2 + 1] = _verticesUVs[i * 4 + 1];
        uv[i * 2] = _verticesUVs[i * 4 + 2] / ts;
        uv[i * 2 + 1] = _verticesUVs[i * 4 + 3] / ts;
    }
    plat::drawStrip(texture ? texture->texture : nullptr, 4, xy, texture ? uv : nullptr, colors, blendMode);
    plat::glPopMatrix();
}

void FlxSprite::render() { renderSprite(); }

// - (BOOL) overlapsPointWithParam1:(float)X param2:(float)Y param3:(BOOL)PerPixel;
BOOL FlxSprite::overlapsPointWithParam1(float X, float Y, BOOL PerPixel) {
    X = X - FlxU::floor(FlxG::scroll()->x);
    Y = Y - FlxU::floor(FlxG::scroll()->y);
    CGPoint _point = getScreenXY();
    if (PerPixel) SDL_Log("overlaps per pixel requested!");
    if ((X <= _point.x) || (X >= _point.x + frameWidth) || (Y <= _point.y) || (Y >= _point.y + frameHeight)) return false;
    return true;
}

// - (void) addAnimation:(NSString *)Name frames:(NSMutableArray *)Frames frameRate:(float)FrameRate looped:(BOOL)Looped;
void FlxSprite::addAnimation(const std::string& Name, const std::vector<unsigned int>& Frames, float FrameRate, BOOL Looped) {
    _animations.push_back((new FlxAnim)->initWithParam1(Name, Frames, FrameRate, Looped));
}

// - (void) playWithParam1:(NSString *)AnimName param2:(BOOL)Force;
// PORT: the original compared NSString pointers (AnimName == _curAnim.name); literals are uniqued so
// string equality is the intended behaviour.
void FlxSprite::play(const std::string& AnimName, BOOL Force) {
    if (!Force && (_curAnim != nullptr) && (AnimName == _curAnim->name)) return;
    _curFrame = 0;
    _caf = 0;
    _frameTimer = 0;
    for (auto* a : _animations) {
        if (a->name == AnimName) {
            _curAnim = a;
            finished = _curAnim->delay <= 0;
            _caf = _curAnim->frames[_curFrame];
            calcFrame();
            return;
        }
    }
}

// - (void) randomFrame;
void FlxSprite::randomFrame() {
    _curAnim = nullptr;
    _caf = ((int)(FlxU::random() * ((texture ? texture->size.width : 0) / frameWidth)));
    calcFrame();
}

// - (void) setFrame:(unsigned int)Frame;
void FlxSprite::setFrame(unsigned int Frame) {
    _curAnim = nullptr;
    _caf = Frame;
    calcFrame();
}

// - (CGPoint) getScreenXY
CGPoint FlxSprite::getScreenXY() {
    CGPoint Point;
    Point.x = std::floor(x + FlxU::roundingError()) + std::floor(FlxG::scroll()->x * scrollFactor.x) - offset.x;
    Point.y = std::floor(y + FlxU::roundingError()) + std::floor(FlxG::scroll()->y * scrollFactor.y) - offset.y;
    return Point;
}

// ============================== FlxManagedSprite ==============================
// -managedInit ran [super initWithX:0 y:0 graphic:nil modelScale:1.0] once per pooled instance,
// then every -initWithX:y:graphic:modelScale: went through -reInitWithX:...
FlxSprite* FlxManagedSprite::initWithX(float X, float Y, const std::string& SimpleGraphic, float ModelScale) {
    FlxSprite::initWithX(0, 0, std::string(), 1.0f);  // managedInit
    exists = true;
    active = true;
    visible = true;
    solid = true;
    fixed = false;
    moves = true;
    collideLeft = collideRight = collideTop = collideBottom = true;
    origin = CGPointZero;
    velocity = CGPointZero;
    acceleration = CGPointZero;
    drag = CGPointZero;
    maxVelocity = CGPointMake(10000, 10000);
    angle = angularVelocity = angularAcceleration = angularDrag = 0;
    maxAngular = 10000;
    thrust = 0;
    scrollFactor = CGPointMake(1, 1);
    _flicker = false;
    _flickerTimer = -1;
    health = 1;
    dead = false;
    colHullX = colHullY = CGRectZero;
    colVector = CGPointZero;
    colOffsets.assign(1, CGPointZero);
    _group = false;
    x = X;
    y = Y;
    width = 0;
    height = 0;
    red = green = blue = 255;
    for (int i = 0; i < 4 * 4; ++i) _colors[i] = 255;
    filled = false;
    predictedX = x;
    predictedY = y;
    modelScale = ModelScale;
    if (SimpleGraphic.empty())
        createGraphicWithParam1(8, 8);
    else
        loadGraphicModelScale(SimpleGraphic, modelScale);
    offset = CGPointZero;
    scale = CGPointMake(1, 1);
    _alpha = 1;
    _color = 0x00ffffff;
    blend.clear();
    antialiasing = false;
    finished = false;
    _facing = RIGHT;
    _animations.clear();
    _flipped = 0;
    _curAnim = nullptr;
    _curFrame = 0;
    _caf = 0;
    _frameTimer = 0;
    _callback = nullptr;
    setupVertices();
    setupTexCoords();
    return this;
}

// ============================== FlxFade ==============================
FlxFade* FlxFade::init() {
    FlxSprite::initWithX(0, 0, std::string(), 1.0f);
    createGraphicWithParam1(FlxG::width(), FlxG::height(), 0xffffffff, true);
    scrollFactor.x = 0;
    scrollFactor.y = 0;
    exists = false;
    return this;
}
// - (void) startWithParam1:(unsigned int)Color param2:(float)Duration param3:(FlashFunction *)FadeComplete param4:(BOOL)Force;
void FlxFade::start(unsigned int Color, float Duration, FlashFunction* FadeComplete, BOOL Force) {
    if (!Force && exists) return;
    fill(Color);
    _delay = Duration;
    _complete = FadeComplete;
    setAlpha(0);
    exists = true;
}
void FlxFade::update() {
    setAlpha(alpha() + FlxG::elapsed() / _delay);
    if (alpha() >= 1) {
        setAlpha(1);
        if (_complete != nullptr) _complete->execute();
    }
}

// ============================== FlxFlash ==============================
FlxFlash* FlxFlash::init() {
    FlxSprite::initWithX(0, 0, std::string(), 1.0f);
    createGraphicWithParam1(FlxG::width(), FlxG::height(), 0xffffffff, true);
    scrollFactor.x = 0;
    scrollFactor.y = 0;
    exists = false;
    return this;
}
void FlxFlash::start(unsigned int Color, float Duration, FlashFunction* FlashComplete, BOOL Force) {
    if (!Force && exists) return;
    fill(Color);
    _delay = Duration;
    _complete = FlashComplete;
    setAlpha(1);
    visible = true;
    exists = true;
}
void FlxFlash::update() {
    setAlpha(alpha() - FlxG::elapsed() / _delay);
    if (alpha() <= 0) {
        exists = false;
        visible = false;
        if (_complete != nullptr) _complete->execute();
    }
}

// ============================== FlxQuake ==============================
FlxQuake* FlxQuake::initWithZoom(float Zoom) {
    scale = CGPointMake(1, 1);
    _zoom = Zoom;
    start(0);
    return this;
}
// - (void) startWithIntensity:(float)Intensity duration:(float)Duration;
float FlxQuake::intensityScale = 1.0f;
void FlxQuake::start(float Intensity, float Duration) {
    stop();
    _intensity = intensityScale == 1.0f ? Intensity : Intensity * intensityScale;
    _timer = Duration;
}
void FlxQuake::stop() {
    x = 0;
    y = 0;
    _intensity = 0;
    _timer = 0;
}
void FlxQuake::update() {
    if (_timer > 0) {
        _timer -= FlxG::elapsed();
        if (_timer <= 0) {
            _timer = 0;
            x = 0;
            y = 0;
        } else {
            x = (FlxU::random() * _intensity * FlxG::width() * 2 - _intensity * FlxG::width()) * _zoom * scale.x;
            y = (FlxU::random() * _intensity * FlxG::height() * 2 - _intensity * FlxG::height()) * _zoom * scale.y;
        }
    }
}

// ============================== FlxTileblock ==============================
FlxTileblock* FlxTileblock::tileblockWithX(float X, float Y, float Width, float Height) {
    return (new FlxTileblock)->initWithX(X, Y, Width, Height);
}
FlxTileblock* FlxTileblock::initWithX(float X, float Y, float Width, float Height) {
    FlxObject::initWithX(X, Y, Width, Height);
    fixed = true;
    verticesUVs.clear();
    refreshHulls();
    return this;
}
// - (id) loadGraphic:(NSString *)TileGraphic empties:(unsigned int)Empties;
FlxTileblock* FlxTileblock::loadGraphic(const std::string& TileGraphic, unsigned int Empties) {
    if (TileGraphic.empty()) {
        SDL_Log("can't load a null graphic");
        return this;
    }
    texture = FlxG::addTextureWithParam1(TileGraphic, false);
    if (texture == nullptr) {
        SDL_Log("couldn't find texture");
        return this;
    }
    tileSize = texture->height();
    unsigned int widthInTiles = ceil(width * 1.0 / tileSize);
    unsigned int heightInTiles = ceil(height * 1.0 / tileSize);
    width = widthInTiles * tileSize;
    height = heightInTiles * tileSize;
    unsigned int numGraphics = texture->width() / tileSize;
    int m = widthInTiles;
    int n = heightInTiles;
    vertexCount = 4 * m + (2 + 4 * m) * (n - 1);
    verticesUVs.assign(vertexCount * 4, 0);
    uShort = FlxGLView::convertToShort(tileSize / texture->paddedWidth());
    vShort = FlxGLView::convertToShort(tileSize / texture->paddedHeight());
    int vi = 0;
    for (int j = 0; j < n; ++j) {
        for (int i = 0; i < m; ++i) {
            if (i == 0 && j != 0) {  // 'stitching'
                verticesUVs[vi] = (GLshort)(m * tileSize);
                verticesUVs[vi + 1] = (GLshort)(j * tileSize);
                verticesUVs[vi + 4] = (GLshort)(0);
                verticesUVs[vi + 5] = (GLshort)(j * tileSize);
                verticesUVs[vi + 2] = verticesUVs[vi + 3] = verticesUVs[vi + 6] = verticesUVs[vi + 7] = 0;
                vi += 8;
            }
            verticesUVs[vi] = (GLshort)(i * tileSize);
            verticesUVs[vi + 1] = (GLshort)(j * tileSize);
            verticesUVs[vi + 4] = (GLshort)(i * tileSize);
            verticesUVs[vi + 5] = (GLshort)((j + 1) * tileSize);
            verticesUVs[vi + 8] = (GLshort)((i + 1) * tileSize);
            verticesUVs[vi + 9] = (GLshort)(j * tileSize);
            verticesUVs[vi + 12] = (GLshort)((i + 1) * tileSize);
            verticesUVs[vi + 13] = (GLshort)((j + 1) * tileSize);
            if (FlxU::random() * (numGraphics + Empties) > Empties || Empties == 0) {
                int gi = FlxU::random() * numGraphics;
                verticesUVs[vi + 2] = gi * uShort;
                verticesUVs[vi + 3] = 0;
                verticesUVs[vi + 6] = gi * uShort;
                verticesUVs[vi + 7] = vShort;
                verticesUVs[vi + 10] = (gi + 1) * uShort;
                verticesUVs[vi + 11] = 0;
                verticesUVs[vi + 14] = (gi + 1) * uShort;
                verticesUVs[vi + 15] = vShort;
            } else {
                verticesUVs[vi + 2] = verticesUVs[vi + 3] = verticesUVs[vi + 6] = verticesUVs[vi + 7] = verticesUVs[vi + 10] =
                    verticesUVs[vi + 11] = verticesUVs[vi + 14] = verticesUVs[vi + 15] = 0;
            }
            vi += 16;
        }
    }
    return this;
}
void FlxTileblock::render() { renderBlock(); }
void FlxTileblock::renderBlock() {
    if (texture == nullptr) return;
    CGPoint _point = getScreenXY();
    plat::glPushMatrix();
    plat::glTranslatef(_point.x, _point.y);
    std::vector<float> xy(vertexCount * 2), uv(vertexCount * 2);
    const float ts = FlxGLView::textureScale();
    for (unsigned int i = 0; i < vertexCount; i++) {
        xy[i * 2] = verticesUVs[i * 4];
        xy[i * 2 + 1] = verticesUVs[i * 4 + 1];
        uv[i * 2] = verticesUVs[i * 4 + 2] / ts;
        uv[i * 2 + 1] = verticesUVs[i * 4 + 3] / ts;
    }
    plat::drawStrip(texture->texture, vertexCount, xy.data(), uv.data(), nullptr, plat::Blend::Normal);
    plat::glPopMatrix();
}
