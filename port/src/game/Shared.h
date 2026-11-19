// Shared contracts for the ZiGGURAT game layer (reconstructed from the iOS binary).
// - State factories: each FlxState subclass lives in its own .cpp; other code only uses these.
// - Statistics: replacement for the CoreData "Statistics" entity (NSManagedObjectContext is gone).
// - L(): replacement for NSLocalizedString using the game's own Localizable.strings tables.
#pragma once
#include <map>
#include <string>

#include "core/GameText.h"
#include "engine/Flixel.h"

// ---------------------------------------------------------------- states
// Original: [[StartState alloc] initWithManagedObjectContext:moc]
FlxState* makeStartState();
// Original: [[TutorialState alloc] initWithManagedObjectContext:moc]
FlxState* makeTutorialState();
// Original: [[OptionsState alloc] initWithManagedObjectContext:moc]
FlxState* makeOptionsState();
// Original: [[StatState alloc] initWithManagedObjectContext:moc]
FlxState* makeStatState();
// Original: [[CreditsState alloc] initWithManagedObjectContext:moc]
FlxState* makeCreditsState();
// Original: [[PlayState alloc] initWithScheme:withAimGuide:withGodMode:withManagedObjectContext:]
//   scheme: 0 = precision, 1 = slingshot (matches preferredControlScheme usage in the binary)
FlxState* makePlayState(int scheme, bool aimGuide, bool godMode);
// Original: [[PlayState alloc] initWithManagedObjectContext:moc] (reads scheme/aimGuide from defaults)
FlxState* makePlayStateDefault();
// Original: [[GameOverState alloc] initWithScore:withImage:withManagedObjectContext:]
//   deathScreen: frame captured by PlayState -snapshot: (may be null)
FlxState* makeGameOverState(unsigned int score, const plat::Texture* deathScreen);

// ---------------------------------------------------------------- statistics
// CoreData model "HighScore", entity "Statistics": every attribute is an NSNumber.
// Access by attribute name exactly as in the model (e.g. "kills", "bestSeconds").
class Statistics {
public:
    static Statistics& shared();        // the single Statistics object (fetch/insert-if-missing)
    double get(const std::string& attr) const;
    int getInt(const std::string& attr) const { return (int)get(attr); }
    float getFloat(const std::string& attr) const { return (float)get(attr); }
    void set(const std::string& attr, double v);
    void save();                        // [managedObjectContext save:]
    void resetAll();                    // StartState -resetStats / deleteAllObjects:
    static const char* const kAttributes[];  // all attribute names of the model (nullptr-terminated)
private:
    std::map<std::string, double> values_;
    bool loaded_ = false;
    void load();
};

// ---------------------------------------------------------------- localization
// NSLocalizedString(key, nil). The key is given by its ID (see core/GameText.h); the text comes from
// the player's dump: assets/strings/<lang>.json, then strings/en.json (whose values are the keys, as
// NSLocalizedString's fallback). Returns an empty string if neither has the entry.
const std::string& L(txt::Id id);
// First preferred language as iOS would report it (e.g. "it", "en", "pt-BR", "ja").
const std::string& preferredLanguage();
