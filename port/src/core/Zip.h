// zigcore: read-only ZIP reader (stored + deflate), enough for .ipa files.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace zig {

class ZipArchive {
public:
    struct Entry {
        std::string name;
        uint16_t method = 0;      // 0 stored, 8 deflate
        uint32_t crc = 0;
        uint64_t compSize = 0, size = 0;
        uint64_t localOffset = 0;
    };
    bool open(const std::string& path);   // reads central directory; false + error() on failure
    const std::vector<Entry>& entries() const { return entries_; }
    const Entry* find(const std::string& name) const;
    bool read(const Entry& e, std::vector<uint8_t>& out) const;
    const std::string& error() const { return error_; }
    static bool looksLikeZip(const std::string& path);  // "PK\3\4" signature

private:
    std::string path_;
    std::vector<Entry> entries_;
    mutable std::string error_;
};

// Raw DEFLATE (no zlib header) into a buffer of known or estimated size.
bool inflateRaw(const uint8_t* in, size_t inLen, std::vector<uint8_t>& out, size_t expectedSize);
uint32_t crc32(const uint8_t* data, size_t len, uint32_t crc = 0);

}  // namespace zig
