// zigcore: UTF-8-safe file access. On Windows, narrow C/C++ file APIs interpret char paths in the
// ANSI code page, so paths with accents/CJK break unless they go through wide APIs. All code that
// opens files by a UTF-8 path should use these helpers.
#pragma once
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace zig {

// UTF-8 string -> filesystem path (wide on Windows).
inline std::filesystem::path u8p(const std::string& s) { return std::filesystem::u8path(s); }

// fopen with a UTF-8 path (uses _wfopen on Windows).
FILE* fopenU8(const std::string& path, const char* mode);

// Read a whole file into memory (UTF-8 path). Returns false if it can't be opened.
bool readFile(const std::string& path, std::vector<uint8_t>& out);

}  // namespace zig
