#include "core/FileIO.h"

#ifdef _WIN32
#include <cwchar>
#endif

namespace zig {

FILE* fopenU8(const std::string& path, const char* mode) {
#ifdef _WIN32
    std::wstring wmode;
    for (const char* m = mode; *m; ++m) wmode += (wchar_t)*m;
    return _wfopen(u8p(path).c_str(), wmode.c_str());
#else
    return fopen(path.c_str(), mode);
#endif
}

bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    out.clear();
    FILE* f = fopenU8(path, "rb");
    if (!f) return false;
    uint8_t buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
    bool ok = !ferror(f);
    fclose(f);
    return ok;
}

}  // namespace zig
