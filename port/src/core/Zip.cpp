#include "core/Zip.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

// stb_image's zlib decoder (implementation compiled once in core/StbImage.cpp).
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace fs = std::filesystem;

namespace zig {
namespace {

FILE* openFile(const std::string& path) {
#ifdef _WIN32
    return _wfopen(fs::u8path(path).wstring().c_str(), L"rb");
#else
    return fopen(path.c_str(), "rb");
#endif
}

uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
uint32_t rd32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

bool readAt(FILE* f, uint64_t off, void* dst, size_t n) {
#ifdef _WIN32
    if (_fseeki64(f, (long long)off, SEEK_SET) != 0) return false;
#else
    if (fseeko(f, (off_t)off, SEEK_SET) != 0) return false;
#endif
    return fread(dst, 1, n, f) == n;
}

}  // namespace

uint32_t crc32(const uint8_t* data, size_t len, uint32_t crc) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        init = true;
    }
    crc = ~crc;
    for (size_t i = 0; i < len; i++) crc = table[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
    return ~crc;
}

bool inflateRaw(const uint8_t* in, size_t inLen, std::vector<uint8_t>& out, size_t expectedSize) {
    int outLen = 0;
    char* res = stbi_zlib_decode_malloc_guesssize_headerflag((const char*)in, (int)inLen,
                                                             (int)(expectedSize ? expectedSize : inLen * 4 + 64),
                                                             &outLen, 0);
    if (!res) return false;
    out.assign((uint8_t*)res, (uint8_t*)res + outLen);
    free(res);
    return true;
}

bool ZipArchive::looksLikeZip(const std::string& path) {
    FILE* f = openFile(path);
    if (!f) return false;
    uint8_t sig[4] = {0};
    bool ok = fread(sig, 1, 4, f) == 4 && sig[0] == 'P' && sig[1] == 'K' && sig[2] == 3 && sig[3] == 4;
    fclose(f);
    return ok;
}

bool ZipArchive::open(const std::string& path) {
    path_ = path;
    entries_.clear();
    FILE* f = openFile(path);
    if (!f) { error_ = "cannot open file"; return false; }
    std::error_code ec;
    uint64_t fileSize = fs::file_size(fs::u8path(path), ec);
    if (ec || fileSize < 22) { fclose(f); error_ = "file too small to be a zip"; return false; }
    // End of central directory: search the last 64 KiB + 22 bytes.
    size_t tailLen = (size_t)std::min<uint64_t>(fileSize, 65557);
    std::vector<uint8_t> tail(tailLen);
    if (!readAt(f, fileSize - tailLen, tail.data(), tailLen)) { fclose(f); error_ = "read error"; return false; }
    long eocd = -1;
    for (long i = (long)tailLen - 22; i >= 0; i--)
        if (rd32(&tail[i]) == 0x06054b50) { eocd = i; break; }
    if (eocd < 0) { fclose(f); error_ = "no zip end-of-directory record (damaged or not a zip)"; return false; }
    uint16_t count = rd16(&tail[eocd + 10]);
    uint32_t cdSize = rd32(&tail[eocd + 12]);
    uint64_t cdOff = rd32(&tail[eocd + 16]);
    if (cdOff + cdSize > fileSize) { fclose(f); error_ = "central directory out of range (truncated file)"; return false; }
    std::vector<uint8_t> cd(cdSize);
    if (!readAt(f, cdOff, cd.data(), cdSize)) { fclose(f); error_ = "read error"; return false; }
    fclose(f);
    size_t p = 0;
    for (uint16_t i = 0; i < count; i++) {
        if (p + 46 > cd.size() || rd32(&cd[p]) != 0x02014b50) { error_ = "corrupt central directory"; return false; }
        Entry e;
        e.method = rd16(&cd[p + 10]);
        e.crc = rd32(&cd[p + 16]);
        e.compSize = rd32(&cd[p + 20]);
        e.size = rd32(&cd[p + 24]);
        uint16_t nameLen = rd16(&cd[p + 28]), extraLen = rd16(&cd[p + 30]), commentLen = rd16(&cd[p + 32]);
        e.localOffset = rd32(&cd[p + 42]);
        if (p + 46 + nameLen > cd.size()) { error_ = "corrupt central directory"; return false; }
        e.name.assign((const char*)&cd[p + 46], nameLen);
        entries_.push_back(std::move(e));
        p += 46 + nameLen + extraLen + commentLen;
    }
    return true;
}

const ZipArchive::Entry* ZipArchive::find(const std::string& name) const {
    for (auto& e : entries_)
        if (e.name == name) return &e;
    return nullptr;
}

bool ZipArchive::read(const Entry& e, std::vector<uint8_t>& out) const {
    FILE* f = openFile(path_);
    if (!f) { error_ = "cannot open file"; return false; }
    uint8_t lh[30];
    if (!readAt(f, e.localOffset, lh, 30) || rd32(lh) != 0x04034b50) {
        fclose(f);
        error_ = "bad local header for " + e.name;
        return false;
    }
    uint64_t dataOff = e.localOffset + 30 + rd16(lh + 26) + rd16(lh + 28);
    std::vector<uint8_t> comp((size_t)e.compSize);
    bool ok = e.compSize == 0 || readAt(f, dataOff, comp.data(), comp.size());
    fclose(f);
    if (!ok) { error_ = "read error in " + e.name; return false; }
    if (e.method == 0) {
        out = std::move(comp);
    } else if (e.method == 8) {
        if (!inflateRaw(comp.data(), comp.size(), out, (size_t)e.size)) { error_ = "inflate failed for " + e.name; return false; }
    } else {
        error_ = "unsupported compression method in " + e.name;
        return false;
    }
    if (out.size() != e.size || crc32(out.data(), out.size()) != e.crc) {
        error_ = "CRC mismatch in " + e.name;
        return false;
    }
    return true;
}

}  // namespace zig
