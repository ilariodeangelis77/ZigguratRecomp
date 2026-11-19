#include "game/Shared.h"
#include "core/FileIO.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

// ---------------------------------------------------------------- statistics
// Attribute list from HighScore.momd (entity "Statistics").
const char* const Statistics::kAttributes[] = {
    "purpleDeaths", "giantDeaths", "giantKills", "facebookPosts", "lastRedKills", "tweets",
    "lastSecondsPlayed", "lastYellowKills", "bestUfoHits", "blueDeaths", "lastGiantKills", "missedShots",
    "ufoHits", "lastShots", "orangeKills", "bestSeconds", "ufoActiveSeconds", "bestMissedShots",
    "lastBlueKills", "bestKills", "bulletDeaths", "moonExplosions", "averageKills", "yellowKills",
    "lastUfoHits", "lastHeadshots", "purpleKills", "averageShots", "orangeDeaths", "secondsPlayed",
    "ufoBestSeconds", "averageSeconds", "lastOrangeKills", "shots", "bigPurpleDeaths", "blueKills",
    "gamesPlayed", "headShots", "lastMissedShots", "lastKills", "redKills", "lastPurpleKills",
    "bestShots", "kills", "bigPurpleKills", "redDeaths", "yellowDeaths", "lastBigPurpleKills",
    nullptr};

Statistics& Statistics::shared() {
    static Statistics s;
    if (!s.loaded_) s.load();
    return s;
}

void Statistics::load() {
    loaded_ = true;
    for (int i = 0; kAttributes[i]; i++) values_[kAttributes[i]] = 0;  // insertNewObject: all nil -> 0
    std::ifstream in(zig::u8p(plat::prefPath("statistics.txt")));
    std::string line;
    while (std::getline(in, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        values_[line.substr(0, eq)] = atof(line.c_str() + eq + 1);
    }
}

double Statistics::get(const std::string& attr) const {
    auto it = values_.find(attr);
    if (it == values_.end()) {
        SDL_Log("Statistics: unknown attribute '%s'", attr.c_str());
        return 0;
    }
    return it->second;
}

void Statistics::set(const std::string& attr, double v) { values_[attr] = v; }

void Statistics::save() {
    std::ofstream out(zig::u8p(plat::prefPath("statistics.txt")));
    char buf[64];
    for (auto& [k, v] : values_) {
        snprintf(buf, sizeof buf, "%.9g", v);
        out << k << '=' << buf << '\n';
    }
}

void Statistics::resetAll() {
    for (auto& [k, v] : values_) v = 0;
    save();
}

// ---------------------------------------------------------------- localization
namespace {

// Minimal parser for the flat {"key": "value"} JSON written by tools/extract_assets.py.
std::map<std::string, std::string> parseFlatJson(const std::string& s) {
    std::map<std::string, std::string> out;
    size_t i = 0;
    auto readStr = [&](std::string& dst) -> bool {
        while (i < s.size() && s[i] != '"') i++;
        if (i >= s.size()) return false;
        i++;
        dst.clear();
        while (i < s.size() && s[i] != '"') {
            char c = s[i++];
            if (c == '\\' && i < s.size()) {
                char e = s[i++];
                switch (e) {
                    case 'n': dst += '\n'; break;
                    case 't': dst += '\t'; break;
                    case 'u': {  // \uXXXX -> UTF-8 (BMP only)
                        unsigned cp = (unsigned)strtoul(s.substr(i, 4).c_str(), nullptr, 16);
                        i += 4;
                        if (cp < 0x80) dst += (char)cp;
                        else if (cp < 0x800) { dst += (char)(0xC0 | (cp >> 6)); dst += (char)(0x80 | (cp & 63)); }
                        else { dst += (char)(0xE0 | (cp >> 12)); dst += (char)(0x80 | ((cp >> 6) & 63)); dst += (char)(0x80 | (cp & 63)); }
                        break;
                    }
                    default: dst += e;
                }
            } else dst += c;
        }
        i++;
        return true;
    };
    std::string k, v;
    while (readStr(k)) {
        while (i < s.size() && s[i] != ':') i++;
        if (!readStr(v)) break;
        out[k] = v;
    }
    return out;
}

std::string gLang;
std::map<txt::Id, std::string> gStrings;  // key ID -> text
bool gLoaded = false;

// Adds every entry of assets/strings/<lang>.json, replacing existing ones. False if the file is missing.
bool mergeStrings(const std::string& lang) {
    std::ifstream in(zig::u8p(plat::assetPath("strings/" + lang + ".json")));
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    for (auto& [k, v] : parseFlatJson(ss.str())) gStrings[txt::hash(k)] = v;
    return true;
}

void loadStrings() {
    gLoaded = true;
    // Like -[NSLocale preferredLanguages]: first preferred locale. Override with ZIGGURAT_LANG.
    if (const char* env = SDL_getenv("ZIGGURAT_LANG")) gLang = env;
    if (gLang.empty()) {
        int n = 0;
        SDL_Locale** locales = SDL_GetPreferredLocales(&n);
        if (locales && n > 0 && locales[0]->language) {
            gLang = locales[0]->language;
            if (gLang == "pt" && locales[0]->country && std::string(locales[0]->country) == "BR") gLang = "pt-BR";
        }
        SDL_free(locales);
    }
    if (gLang.empty()) gLang = "en";
    // English first (CFBundleDevelopmentRegion; its values equal the keys, which is what
    // NSLocalizedString returns for an untranslated key), then the preferred language on top.
    if (!mergeStrings("en")) SDL_Log("strings: en.json not found in the imported game files");
    if (gLang != "en") mergeStrings(gLang);
}

}  // namespace

const std::string& L(txt::Id id) {
    static const std::string kMissing;
    if (!gLoaded) loadStrings();
    auto it = gStrings.find(id);
    if (it != gStrings.end()) return it->second;
    SDL_Log("strings: no entry for text id 0x%08x", (unsigned)id);
    return kMissing;
}

const std::string& preferredLanguage() {
    if (!gLoaded) loadStrings();
    return gLang;
}
