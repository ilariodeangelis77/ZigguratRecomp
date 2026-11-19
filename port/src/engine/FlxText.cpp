//
//  FlxText.cpp — FlxText.m
//
//  Copyright Semi Secret Software 2009-2010. All rights reserved.
//  C++ port: MIT license (see ENGINE_LICENSE.txt).
//
#include "Flixel.h"

float FlxText::getHeight() {
    if (regen) setupNewText();
    return FlxSprite::getHeight();
}

// + (id) textWithWidth:(unsigned int)Width text:(NSString *)Text font:(NSString *)Font size:(float)Size modelScale:(float)ModelScale;
// (Size == 0 selects the +textWithWidth:text: variant, which leaves font and size untouched)
FlxText* FlxText::textWithWidth(unsigned int Width, const std::string& Text, const std::string& Font, float Size, float ModelScale) {
    FlxText* ret = (new FlxText)->initWithX(0, 0, Width, Text, ModelScale);
    if (Size != 0 || !Font.empty()) {
        ret->setSize(Size);
        ret->setFont(Font);
    }
    return ret;
}

// PORT: -dealloc released the text's own texture (textureWithView, never cached) and SemiSecretText.
FlxText::~FlxText() {
    delete texture;
    texture = nullptr;
    delete sstext;
    sstext = nullptr;
}

// - (id) initWithX:(float)X y:(float)Y width:(unsigned int)Width text:(NSString *)Text modelScale:(float)ModelScale;
FlxText* FlxText::initWithX(float X, float Y, unsigned int Width, const std::string& Text, float ModelScale) {
    FlxSprite::initWithX(X, Y, std::string(), ModelScale);
    regen = true;
    width = Width;
    sstext = (new SemiSecretText)->initWithFrame(CGRectZero);
    sstext->setPadding(sstext->padding * modelScale);
    sstext->reposition = false;
    sstext->setText(toU16(Text));
    sstext->wrapped = true;
    SemiSecretFont* f = sstext->font();
    f = sstext->font()->fontWithSize(f->size * modelScale);
    sstext->setFont(f);
    sstext->align = SemiSecretTextAlignLeft;
    filled = false;
    return this;
}

// - (FlxText *) setFormatWithParam1:param2:param3:param4:param5:
FlxText* FlxText::setFormatWithParam1(const std::string& Font, float Size, unsigned int Color, const std::string& Alignment,
                                      unsigned int ShadowColor) {
    if (!Font.empty()) {
        SemiSecretFont* f = SemiSecretFont::fontWithName(Font, Size * modelScale);
        sstext->setFont(f);
    } else if (Size) {
        SemiSecretFont* f = sstext->font()->fontWithSize(Size * modelScale);
        sstext->setFont(f);
    }
    if (Color) setColor(Color);
    if (!Alignment.empty()) setAlignment(Alignment);
    if (ShadowColor) setShadow(ShadowColor);
    regen = true;
    return this;
}

std::string FlxText::text() { return toU8(sstext->text); }
void FlxText::setText(const std::string& Text) {
    sstext->setText(toU16(Text));
    regen = true;
}
float FlxText::size() { return (float)(sstext->font()->size) / modelScale; }
void FlxText::setSize(float Size) {
    sstext->setFont(sstext->font()->fontWithSize(Size * modelScale));
    regen = true;
}
unsigned int FlxText::color() {
    unsigned int c = 0;
    c |= (((int)(sstext->color.r * 255)) << 16);
    c |= (((int)(sstext->color.g * 255)) << 8);
    c |= (((int)(sstext->color.b * 255)) << 0);
    return c;
}
void FlxText::setColor(unsigned int Color) {
    float r = ((Color >> 16) & 0xff) / 255.0;
    float g = ((Color >> 8) & 0xff) / 255.0;
    float b = ((Color >> 0) & 0xff) / 255.0;
    sstext->color = CGColor{r, g, b, 1.0f};
    regen = true;
}
std::string FlxText::font() {
    std::string fontClass = sstext->font()->className();
    if (fontClass.size() >= 4 && fontClass.compare(fontClass.size() - 4, 4, "Font") == 0) fontClass.erase(fontClass.size() - 4);
    return fontClass;
}
void FlxText::setFont(const std::string& FontIn) {
    std::string Font = FontIn.empty() ? "Flixel" : FontIn;
    sstext->setFont(SemiSecretFont::fontWithName(Font, sstext->font() ? sstext->font()->size : 8));
    regen = true;
}
std::string FlxText::alignment() {
    switch (sstext->align) {
        case SemiSecretTextAlignLeft: return "left";
        case SemiSecretTextAlignCenter: return "center";
        case SemiSecretTextAlignRight: return "right";
    }
    return "left";
}
void FlxText::setAlignment(const std::string& Alignment) {
    if (!Alignment.empty()) {
        if (Alignment == "center") sstext->align = SemiSecretTextAlignCenter;
        else if (Alignment == "justify") sstext->align = SemiSecretTextAlignLeft;  // not yet implemented
        else if (Alignment == "left") sstext->align = SemiSecretTextAlignLeft;
        else if (Alignment == "right") sstext->align = SemiSecretTextAlignRight;
        else sstext->align = SemiSecretTextAlignLeft;
    } else
        sstext->align = SemiSecretTextAlignLeft;
    regen = true;
}
unsigned int FlxText::shadow() {
    unsigned int c = 0;
    c |= (((int)(sstext->shadowColor.r * 255)) << 16);
    c |= (((int)(sstext->shadowColor.g * 255)) << 8);
    c |= (((int)(sstext->shadowColor.b * 255)) << 0);
    return c;
}
void FlxText::setShadow(unsigned int ShadowColor) {
    if (ShadowColor > 0) {
        float r = ((ShadowColor >> 16) & 0xff) / 255.0;
        float g = ((ShadowColor >> 8) & 0xff) / 255.0;
        float b = ((ShadowColor >> 0) & 0xff) / 255.0;
        sstext->effect = SemiSecretTextEffectBlurredShadow;
        sstext->shadowColor = CGColor{r, g, b, 1.0f};
    } else {
        sstext->effect = SemiSecretTextEffectNone;
    }
    regen = true;
}

// - (void) setupNewText;
void FlxText::setupNewText() {
    float Height = sstext->heightForLines(-1, width * modelScale);
    height = Height / modelScale;
    sstext->setFrame(CGRectMake(0, 0, width * modelScale, height * modelScale));
    if (texture) {
        delete texture;
        texture = nullptr;
    }
    texture = SemiSecretTexture::textureWithView(sstext);
    frameWidth = width;
    frameHeight = height;
    resetHelpers();
    regen = false;
}

void FlxText::render() {
    if (regen) setupNewText();
    FlxSprite::render();
}

// - (void) sizeToFit
void FlxText::sizeToFit() {
    sstext->computeNewBounds();
    width = sstext->bounds.size.width / modelScale;
    setupNewText();
}
