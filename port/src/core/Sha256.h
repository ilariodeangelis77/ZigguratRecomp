// zigcore: minimal SHA-256 (FIPS 180-4).
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace zig {

class Sha256 {
public:
    Sha256();
    void update(const void* data, size_t len);
    void final(uint8_t out[32]);
    std::string hex();  // finalizes
    static std::string hexOf(const void* data, size_t len);

private:
    void block(const uint8_t* p);
    uint32_t h_[8];
    uint8_t buf_[64];
    size_t bufLen_ = 0;
    uint64_t total_ = 0;
};

}  // namespace zig
