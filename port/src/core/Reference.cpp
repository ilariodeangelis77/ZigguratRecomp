#include "core/Reference.h"

#include <SDL3/SDL.h>

#include "core/Process.h"

#include <filesystem>
#include <fstream>

#include "core/Paths.h"
#include "core/Zip.h"

#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace zig {

std::string Reference::findTouchHLE(const std::string& configured) {
    if (!configured.empty() && fileExists(configured)) return configured;
#ifdef _WIN32
    const char* exe = "touchHLE_zig.exe";
#else
    const char* exe = "touchHLE_zig";
#endif
    for (std::string rel : {"../../_ext/touchHLE/", "../_ext/touchHLE/", "touchHLE/"}) {
        std::error_code ec;
        fs::path p = fs::weakly_canonical(fs::u8path(Paths::exeDir() + rel + exe), ec);
        if (!ec && fileExists(p.u8string())) return p.u8string();
    }
    return {};
}

std::string Reference::appFolder(const std::string& src, std::string& err) {
    if (src.empty()) { err = "no dump configured"; return {}; }
    if (dirExists(src)) {
        if (fileExists(src + "/Info.plist")) return src;
        if (fileExists(src + "/Payload/Zig2.app/Info.plist")) return src + "/Payload/Zig2.app";
        err = "folder does not contain Zig2.app";
        return {};
    }
    std::string dest = Paths::dataDir() + "reference/Zig2.app";
    if (fileExists(dest + "/Info.plist")) return dest;
    ZipArchive z;
    if (!z.open(src)) { err = z.error(); return {}; }
    const std::string prefix = "Payload/Zig2.app/";
    int n = 0;
    for (auto& e : z.entries()) {
        if (e.name.rfind(prefix, 0) != 0 || e.name.back() == '/') continue;
        std::string rel = e.name.substr(prefix.size());
        if (rel.find("..") != std::string::npos) continue;   // never escape the target folder
        std::vector<uint8_t> data;
        if (!z.read(e, data)) { err = z.error(); return {}; }
        fs::path out = fs::u8path(dest + "/" + rel);
        std::error_code ec;
        fs::create_directories(out.parent_path(), ec);
        std::ofstream o(out, std::ios::binary | std::ios::trunc);
        o.write((const char*)data.data(), (std::streamsize)data.size());
        if (!o) { err = "cannot write " + out.u8string(); return {}; }
        n++;
    }
    if (n == 0) { err = "Payload/Zig2.app not found in the IPA"; return {}; }
    SDL_Log("reference: extracted %d files to %s", n, dest.c_str());
    return dest;
}

bool Reference::launch(const std::string& exe, const std::string& app, std::string& err) {
    fs::path dir = fs::u8path(exe).parent_path();
#ifdef _WIN32
    // touchHLE finds touchHLE_dylibs/ and touchHLE_fonts/ relative to its working directory.
    std::wstring cmd = L"\"" + fs::u8path(exe).wstring() + L"\" \"" + fs::u8path(app).wstring() + L"\" --landscape-left";
    std::wstring wd = dir.wstring();
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(0);
    if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, 0, nullptr, wd.c_str(), &si, &pi)) {
        err = "CreateProcess failed (" + std::to_string(GetLastError()) + ")";
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);   // detached: we don't wait for touchHLE
#else
    std::error_code ec;
    fs::path old = fs::current_path(ec);
    fs::current_path(dir, ec);
    const char* args[] = {exe.c_str(), app.c_str(), "--landscape-left", nullptr};
    SDL_Process* p = zig::spawnProcess(args);
    fs::current_path(old, ec);
    if (!p) { err = SDL_GetError(); return false; }
    SDL_DestroyProcess(p);
#endif
    SDL_Log("reference: started %s", exe.c_str());
    return true;
}

}  // namespace zig
