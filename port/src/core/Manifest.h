// zigcore: known-good file list of a ZiGGURAT dump (name, size, SHA-256 per file; no content).
#pragma once
#include <cstdint>
#include <string>

namespace zig {

struct ManifestEntry {
    const char* path;      // relative to Zig2.app/ ("iTunesArtwork" = IPA root)
    uint32_t size;
    const char* sha256;    // lowercase hex
    bool optional;         // not required for the game (iTunesArtwork, only used for Steam/desktop icons)
};

struct Manifest {
    static const char* version();      // CFBundleShortVersionString of the reference dump
    static const char* bundleId();     // "com.freshuu.ziggurat"
    static int count();
    static const ManifestEntry& at(int i);
    static const ManifestEntry* find(const std::string& path);
};

}  // namespace zig
