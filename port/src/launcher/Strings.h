// Launcher UI strings (LAUNCHER_PLAN.md §7.5). Built-in English table; translations are loaded at
// runtime from <exe>/strings/<lang>.json (same ids). Missing ids fall back to English.
#pragma once
#include <string>
#include <vector>

namespace lz {

// code: "system" or one of en de es fr it pt pt-BR. Returns the resolved code.
std::string setLanguage(const std::string& code);
const std::string& language();
// Lookup by id; unknown id returns the id itself.
const std::string& T(const char* id);
// Lookup with positional arguments {0} {1} ...
std::string Tf(const char* id, const std::vector<std::string>& args);
bool has(const std::string& id);
// Writes the built-in English table as JSON (used to (re)generate strings/en.json).
bool writeEnglish(const std::string& file);

// Game languages offered in the LANGUAGE tab (code, native name).
struct Lang { const char* code; const char* name; };
const std::vector<Lang>& languages();

}  // namespace lz
