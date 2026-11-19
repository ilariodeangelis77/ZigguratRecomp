// zigcore: locate, verify and import a ZiGGURAT dump (LAUNCHER_PLAN.md §7.1).
// Thread-safe to call from a worker thread (no global state besides read-only manifest).
// Accepted inputs: .ipa file, folder containing an .ipa, Payload/<X>.app or any <X>.app folder,
// or a folder containing any of these (recursive, depth <= 3). Encrypted App Store IPAs work:
// only the executable is FairPlay-encrypted; the assets the port uses are not.
#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace zig {

enum class DumpStatus {
    Verified,        // bundle id + all files match the manifest
    UnknownVersion,  // bundle id ok, all required files present, but version/hashes differ -> "import anyway"
    NotZiggurat,     // not an iOS app, or a different app
    Incomplete,      // required files missing
    Modified,        // known version but files altered
    Unreadable,      // I/O error, damaged zip, permission denied
    Compressed,      // .zip/.rar/.7z wrapper instead of the .ipa
    NotFound,        // path does not exist / nothing found while scanning
};
const char* toString(DumpStatus s);

// Translatable message: id + positional args; `text` is the English rendering.
struct DumpMessage {
    std::string id;                 // e.g. "dump.incomplete"
    std::vector<std::string> args;  // e.g. {"3", "scream.wav, gameOver.wav, shoot.wav"}
    std::string text;               // English
};

struct DumpCandidate {
    enum class Kind { Ipa, AppDir } kind = Kind::Ipa;
    std::string path;    // .ipa file or .app directory
    std::string label;   // short display name
};

struct ScanResult {
    std::vector<DumpCandidate> candidates;  // best first
    DumpStatus status = DumpStatus::NotFound;  // meaningful when candidates is empty
    DumpMessage message;
};

struct DumpInfo {
    bool ok = false;
    std::string bundleId, version, displayName;
    std::vector<std::string> files;  // paths relative to the .app (+ "iTunesArtwork" if present)
    DumpMessage error;
};

enum class FileState { Ok, Missing, Modified };
struct FileCheck {
    std::string path;
    FileState state = FileState::Ok;
    bool optional = false;
};

struct VerifyResult {
    DumpStatus status = DumpStatus::Unreadable;
    DumpMessage message;
    std::string bundleId, version;
    int total = 0, ok = 0, missing = 0, modified = 0;  // over manifest entries
    std::vector<FileCheck> files;                      // one per manifest entry
};

struct ImportResult {
    bool ok = false;
    DumpMessage message;
    std::string assetsDir;
    int pngs = 0, audio = 0, fonts = 0, strings = 0, other = 0;
    uint64_t bytesWritten = 0;
    double seconds = 0;
};

// done/total units are files; item = file currently processed.
using DumpProgress = std::function<void(int done, int total, const std::string& item)>;

ScanResult scanForDumps(const std::string& path);
DumpInfo inspectDump(const DumpCandidate& c);
VerifyResult verifyDump(const DumpCandidate& c, const DumpProgress& progress = nullptr);
// Imports into destAssetsDir (written to destAssetsDir + ".tmp", then renamed into place).
// allowUnverified: proceed even when the version is unknown (hashes differ).
ImportResult importDump(const DumpCandidate& c, const std::string& destAssetsDir, const DumpProgress& progress = nullptr,
                        const std::atomic<bool>* cancel = nullptr, bool allowUnverified = false);

constexpr uint64_t kImportSpaceNeeded = 25ull * 1024 * 1024;

// Exposed for tests: Apple CgBI ("crushed") PNG -> standard PNG. Non-CgBI input is returned unchanged.
bool convertCgbiPng(const std::vector<uint8_t>& in, std::vector<uint8_t>& out, std::string* err = nullptr);
// Localizable.strings (binary plist, XML plist or text .strings) -> JSON object text.
bool stringsToJson(const std::vector<uint8_t>& in, std::string& json, std::string* err = nullptr);
// Binary plist (bplist00, nested arrays/dicts) -> JSON text. Used by the stats importers (L7).
bool binaryPlistToJson(const std::vector<uint8_t>& in, std::string& json);

}  // namespace zig
