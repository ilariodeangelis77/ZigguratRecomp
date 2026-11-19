// Game text IDs.
//
// None of the game's on-screen text is stored in this project. It is read at runtime from the
// player's own copy: the import step converts the game's Localizable.strings tables into
// assets/strings/<lang>.json, keyed by the original English lookup key. The code refers to each
// entry only by an ID: the 32-bit FNV-1a hash of that key (UTF-8).
//
// tools/check_game_text.py verifies the IDs against an imported dump, and fails the build if the
// text itself appears in the source. Entries marked "generic" are short UI words that the launcher
// also uses in its own strings; they are only forbidden inside port/src/game/.
#pragma once
#include <cstdint>
#include <string_view>

namespace txt {

using Id = std::uint32_t;

constexpr Id hash(std::string_view s) {
    std::uint32_t h = 2166136261u;
    for (unsigned char c : s) {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}

// ---- credits
constexpr Id kCreditsTitle = 0x48219d68;  // generic
constexpr Id kCreditsByline = 0x72f469a8;
constexpr Id kCreditsStaff = 0x675ca4d1;
constexpr Id kCreditsDirector = 0x6a15d682;
constexpr Id kCreditsProgram = 0x5f99e3ab;
constexpr Id kCreditsGraphics = 0x21519788;
constexpr Id kCreditsSound = 0xb67736f4;
constexpr Id kCreditsThanks = 0xf8bd4e79;
constexpr Id kCreditsWebsite = 0x90695905;

// ---- game over
constexpr Id kOverShareStart = 0x11390344;
constexpr Id kOverShareEnd = 0x75ea8be0;
constexpr Id kOverScoreStart = 0xdd1180f8;
constexpr Id kOverScoreEnd = 0x14fae511;
constexpr Id kOverTweet = 0xc9b52c86;
constexpr Id kOverFacebook = 0x92c4c12b;
constexpr Id kOverRetry = 0xb4fabf47;

// ---- options
constexpr Id kOptTitle = 0x31542cc5;
constexpr Id kOptPrecision = 0x73e68d42;
constexpr Id kOptSlingshot = 0xa7468cb1;
constexpr Id kOptAimGuide = 0x9673ec16;  // generic
constexpr Id kOptOff = 0x2e1505ea;  // generic
constexpr Id kOptOn = 0x80e4b050;  // generic
constexpr Id kOptHowToPlay = 0x4b6096d8;
constexpr Id kOptViewStats = 0xdbde9a0b;
constexpr Id kOptFacebook = 0x8e8a53b1;
constexpr Id kOptTwitter = 0x8f57d524;
constexpr Id kOptPlayNow = 0x1b7f577b;  // generic

// ---- statistics
constexpr Id kStatTotal = 0xa7d7b27b;
constexpr Id kStatAverage = 0xaa647632;
constexpr Id kStatBest = 0x4328f4fb;
constexpr Id kStatDeathsBy = 0xf3378041;
constexpr Id kStatKills = 0x46cf6a6e;
constexpr Id kStatGames = 0x9fb1ba25;
constexpr Id kStatSeconds = 0x80ac6168;
constexpr Id kStatShots = 0x20222dae;
constexpr Id kStatAccuracy = 0x2555bed8;
constexpr Id kStatHeadshots = 0x3606921c;
constexpr Id kStatMoon = 0xbcaea46c;
constexpr Id kStatBlueKills = 0x47f30fc0;
constexpr Id kStatYellowKills = 0xd16f19c0;
constexpr Id kStatRedKills = 0x120ebd17;
constexpr Id kStatOrangeKills = 0x194b592e;
constexpr Id kStatGiantKills = 0xa4e1133c;
constexpr Id kStatPurpleKills = 0x47233ff6;
constexpr Id kStatBigPurpleKills = 0x5a28fae4;
constexpr Id kStatAvgKills = 0x9ee8b3c7;
constexpr Id kStatAvgSeconds = 0x6ccabacd;
constexpr Id kStatAvgShots = 0x907db607;
constexpr Id kStatBestKills = 0x55c4778c;
constexpr Id kStatBestSeconds = 0xb3ea412c;
constexpr Id kStatBestShots = 0xb32d5cd0;
constexpr Id kStatBestAccuracy = 0x6cc3dfda;
constexpr Id kStatUfoHits = 0xe283e319;
constexpr Id kStatBestUfoHits = 0x71002dfb;
constexpr Id kStatUfoKills = 0xa2e55d58;
constexpr Id kStatBulletDeaths = 0xd52b7bf3;
constexpr Id kStatBlueDeaths = 0x25865d19;
constexpr Id kStatYellowDeaths = 0x00485b19;
constexpr Id kStatRedDeaths = 0xa60fcbe4;
constexpr Id kStatOrangeDeaths = 0x0a7adb77;
constexpr Id kStatGiantDeaths = 0x74ba8b2d;
constexpr Id kStatPurpleDeaths = 0xee2e72af;
constexpr Id kStatBigPurpleDeaths = 0x80695c25;
constexpr Id kStatTweet = 0x0e03222e;
constexpr Id kStatFacebook = 0xf0287223;
constexpr Id kStatCredits = 0x5bdcf3ca;

// ---- number formats (printf, one %i)
constexpr Id kFmtGames = 0x2e9e8a98;
constexpr Id kFmtKills = 0x968bd242;
constexpr Id kFmtSeconds = 0x7d73e20e;
constexpr Id kFmtShots = 0xaa55aae6;
constexpr Id kFmtHeadshots = 0x8a911164;

// ---- shared
constexpr Id kBack = 0x5bb421a2;  // generic

// ---- tutorial
constexpr Id kTutTouchHere = 0x967c6876;
constexpr Id kTutDontTouch = 0x963370ec;
constexpr Id kTutSlideToAim = 0x9e5b0b76;
constexpr Id kTutHoldToCharge = 0x011342e3;
constexpr Id kTutReleaseToFire = 0x3949f3d1;
constexpr Id kTutShootEyes = 0xc976482b;
constexpr Id kTutSlingshotHint = 0xcc58c3bb;
constexpr Id kTutViewAgain = 0x45bdb569;
constexpr Id kTutOk = 0xd50dbe0a;  // generic

}  // namespace txt
