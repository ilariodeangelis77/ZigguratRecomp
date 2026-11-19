// zigcore: Valve KeyValues ("VDF") in two flavours.
//  - Binary (shortcuts.vdf): lossless parse/write, byte-identical round trip, unknown fields kept.
//  - Text (loginusers.vdf, libraryfolders.vdf): read-only tree.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace zig::vdf {

// ---------------------------------------------------------------- binary
enum class BType : uint8_t {
    Map = 0x00, String = 0x01, Int32 = 0x02, Float32 = 0x03, Ptr = 0x04, WString = 0x05, Color = 0x06,
    UInt64 = 0x07, End = 0x08, Int64 = 0x0A,
};

struct BNode {
    BType type = BType::Map;
    std::string key;
    std::string str;          // String
    uint32_t u32 = 0;         // Int32 / Float32 / Ptr / Color (raw bits)
    uint64_t u64 = 0;         // UInt64 / Int64
    std::vector<BNode> kids;  // Map

    // Case-insensitive child lookup (Steam writes "AppName"/"appname", "Exe"/"exe"...).
    BNode* find(const std::string& k);
    const BNode* find(const std::string& k) const;
    std::string getStr(const std::string& k, const std::string& def = "") const;
    int32_t getInt(const std::string& k, int32_t def = 0) const;
    // Set (creating at the end if missing; existing key keeps its position and spelling).
    BNode& setStr(const std::string& k, const std::string& v);
    BNode& setInt(const std::string& k, int32_t v);
    BNode& setMap(const std::string& k);

    static BNode str_(const std::string& k, const std::string& v);
    static BNode int_(const std::string& k, int32_t v);
};

// Root = implicit map terminated by End. Returns false (and an error) on malformed input.
bool parseBinary(const std::vector<uint8_t>& data, BNode& root, std::string* err = nullptr);
std::vector<uint8_t> writeBinary(const BNode& root);

// An empty shortcuts.vdf as Steam writes it: "\0shortcuts\0\x08\x08".
BNode emptyShortcuts();

// ---------------------------------------------------------------- text
struct TNode {
    std::string key, value;   // leaf if kids empty and isMap == false
    bool isMap = false;
    std::vector<TNode> kids;
    const TNode* find(const std::string& k) const;  // case-insensitive
    std::string get(const std::string& k, const std::string& def = "") const;
};
bool parseText(const std::string& text, TNode& root, std::string* err = nullptr);

bool readFile(const std::string& path, std::vector<uint8_t>& out);

}  // namespace zig::vdf
