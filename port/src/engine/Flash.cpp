//
//  Flash.cpp — FlashPoint / FlashRectangle (src/Flash/*.m) + NSUserDefaults replacement + string helpers
//
//  Copyright Semi Secret Software 2009-2010. All rights reserved.
//  C++ port: MIT license (see ENGINE_LICENSE.txt).
//
#include <algorithm>
#include "core/FileIO.h"
#include <cstdio>
#include <fstream>
#include <sstream>

#include "Flixel.h"

// ---------------- FlashPoint ----------------
// - (FlashPoint *) add:(FlashPoint *)v;
FlashPoint* FlashPoint::add(FlashPoint* v) { return (new FlashPoint)->initWithX(x + v->x, y + v->y); }
// + (float) distanceWithParam1:(FlashPoint *)pt1 param2:(FlashPoint *)pt2;
float FlashPoint::distanceWithParam1(FlashPoint* pt1, FlashPoint* pt2) {
    return sqrt((pt1->x - pt2->x) * (pt1->x - pt2->x) + (pt1->y - pt2->y) * (pt1->y - pt2->y));
}
// + (FlashPoint *) interpolateWithParam1:(FlashPoint *)pt1 param2:(FlashPoint *)pt2 param3:(float)f;
FlashPoint* FlashPoint::interpolateWithParam1(FlashPoint* pt1, FlashPoint* pt2, float f) {
    return (new FlashPoint)->initWithX((pt1->x - pt2->x) * f + pt2->x, (pt1->y - pt2->y) * f + pt2->y);
}
// - (float) length   (sic: y+y in the original)
float FlashPoint::length() { return sqrt(x * x + y + y); }
// - (void) normalize:(float)thickness;
void FlashPoint::normalize(float thickness) {
    float s = thickness / length();
    x *= s;
    y *= s;
}
// + (FlashPoint *) polarWithParam1:(float)len param2:(float)angle;
FlashPoint* FlashPoint::polarWithParam1(float len, float angle) { return (new FlashPoint)->initWithX(len * cos(angle), len * sin(angle)); }
// - (FlashPoint *) subtract:(FlashPoint *)v;
FlashPoint* FlashPoint::subtract(FlashPoint* v) { return (new FlashPoint)->initWithX(x - v->x, y - v->y); }

// ---------------- FlashRectangle ----------------
// - (FlashRectangle *) intersection:(FlashRectangle *)toIntersect;  (sic: uses toIntersect.top for left)
FlashRectangle* FlashRectangle::intersection(FlashRectangle* r) {
    auto* ret = (new FlashRectangle)->initWithX();
    ret->setLeft(std::max(left(), r->top()));
    ret->setTop(std::max(top(), r->top()));
    ret->setRight(std::min(right(), r->right()));
    ret->setBottom(std::min(bottom(), r->bottom()));
    return ret;
}
bool FlashRectangle::intersects(FlashRectangle* r) { return !intersection(r)->isEmpty(); }
// - (FlashRectangle *) union:(FlashRectangle *)toUnion;
FlashRectangle* FlashRectangle::union_(FlashRectangle* r) {
    auto* ret = (new FlashRectangle)->initWithX();
    ret->setLeft(std::min(left(), r->top()));
    ret->setTop(std::min(top(), r->top()));
    ret->setRight(std::max(right(), r->right()));
    ret->setBottom(std::max(bottom(), r->bottom()));
    return ret;
}

// ---------------- UserDefaults (NSUserDefaults) ----------------
// PORT: values are stored as strings in a tab-separated file in the SDL pref path.
UserDefaults& UserDefaults::standard() {
    static UserDefaults d;
    if (!d.loaded) d.load();
    return d;
}
void UserDefaults::load() {
    loaded = true;
    std::ifstream f(zig::u8p(plat::prefPath("defaults.tsv")));
    std::string line;
    while (std::getline(f, line)) {
        auto tab = line.find('\t');
        if (tab != std::string::npos) values[line.substr(0, tab)] = line.substr(tab + 1);
    }
}
bool UserDefaults::synchronize() {
    std::ofstream f(zig::u8p(plat::prefPath("defaults.tsv")), std::ios::trunc);
    for (auto& [k, v] : values) f << k << '\t' << v << '\n';
    return (bool)f;
}
bool UserDefaults::has(const std::string& key) { return values.count(key) || defaults.count(key); }
std::string UserDefaults::get(const std::string& key, const std::string& def) {
    auto it = values.find(key);
    if (it != values.end()) return it->second;
    auto d = defaults.find(key);
    return d != defaults.end() ? d->second : def;
}
float UserDefaults::getFloat(const std::string& key, float def) {
    std::string s = get(key);
    return s.empty() ? def : (float)atof(s.c_str());
}
bool UserDefaults::getBool(const std::string& key, bool def) {
    std::string s = get(key);
    if (s.empty()) return def;
    return s == "1" || s == "YES" || s == "true" || atof(s.c_str()) != 0;
}
void UserDefaults::set(const std::string& key, const std::string& value) { values[key] = value; }
void UserDefaults::setFloat(const std::string& key, float v) {
    char b[64];
    snprintf(b, sizeof b, "%.9g", v);
    values[key] = b;
}
void UserDefaults::setBool(const std::string& key, bool v) { values[key] = v ? "1" : "0"; }
void UserDefaults::remove(const std::string& key) { values.erase(key); }
void UserDefaults::registerDefault(const std::string& key, const std::string& value) { defaults[key] = value; }
std::vector<std::string> UserDefaults::keysWithPrefix(const std::string& prefix) {
    std::vector<std::string> r;
    for (auto& [k, v] : values)
        if (k.compare(0, prefix.size(), prefix) == 0) r.push_back(k);
    return r;
}

// ---------------- UTF helpers (NSString is UTF-16) ----------------
std::u16string toU16(const std::string& s) {
    std::u16string out;
    for (size_t i = 0; i < s.size();) {
        uint32_t c = (unsigned char)s[i];
        int n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        if (n == 2) c = ((c & 0x1F) << 6) | (s[i + 1] & 0x3F);
        else if (n == 3) c = ((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F);
        else if (n == 4) c = ((c & 0x07) << 18) | ((s[i + 1] & 0x3F) << 12) | ((s[i + 2] & 0x3F) << 6) | (s[i + 3] & 0x3F);
        i += n;
        if (c >= 0x10000) {
            c -= 0x10000;
            out.push_back((char16_t)(0xD800 + (c >> 10)));
            out.push_back((char16_t)(0xDC00 + (c & 0x3FF)));
        } else out.push_back((char16_t)c);
    }
    return out;
}
std::string toU8(const std::u16string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        uint32_t c = s[i];
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < s.size()) c = 0x10000 + ((c - 0xD800) << 10) + (s[++i] - 0xDC00);
        if (c < 0x80) out += (char)c;
        else if (c < 0x800) { out += (char)(0xC0 | (c >> 6)); out += (char)(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) { out += (char)(0xE0 | (c >> 12)); out += (char)(0x80 | ((c >> 6) & 0x3F)); out += (char)(0x80 | (c & 0x3F)); }
        else { out += (char)(0xF0 | (c >> 18)); out += (char)(0x80 | ((c >> 12) & 0x3F)); out += (char)(0x80 | ((c >> 6) & 0x3F)); out += (char)(0x80 | (c & 0x3F)); }
    }
    return out;
}
