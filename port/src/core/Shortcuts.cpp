#include "core/Shortcuts.h"
#include "core/TestHooks.h"

#include <SDL3/SDL.h>

#include <filesystem>
#include <fstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>
#endif

namespace fs = std::filesystem;

namespace zig {
namespace {
#ifdef _WIN32
std::wstring widen(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n > 0 ? n - 1 : 0, L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}
std::string narrow(const wchar_t* w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}
std::string knownFolder(REFKNOWNFOLDERID id) {
    PWSTR p = nullptr;
    std::string out;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p))) out = narrow(p);
    CoTaskMemFree(p);
    return out;
}
#endif
}  // namespace

static std::string gDirOverride;
void Shortcuts::setDirOverride(const std::string& dir) { gDirOverride = dir; }

std::string Shortcuts::dirFor(ShortcutPlace p) {
    if (!gDirOverride.empty()) return gDirOverride + (p == ShortcutPlace::Desktop ? "/Desktop" : "/StartMenu");
    if (const char* o = zig::testEnv("ZIGGURAT_SHORTCUT_DIR"); o && *o)
        return std::string(o) + (p == ShortcutPlace::Desktop ? "/Desktop" : "/StartMenu");
#ifdef _WIN32
    return p == ShortcutPlace::Desktop ? knownFolder(FOLDERID_Desktop) : knownFolder(FOLDERID_Programs);
#else
    const char* home = SDL_getenv("HOME");
    if (!home) return "";
    if (p == ShortcutPlace::StartMenu) {
        const char* xdg = SDL_getenv("XDG_DATA_HOME");
        return (xdg && *xdg ? std::string(xdg) : std::string(home) + "/.local/share") + "/applications";
    }
    return std::string(home) + "/Desktop";
#endif
}

std::string Shortcuts::pathFor(ShortcutPlace p) {
    std::string d = dirFor(p);
    if (d.empty()) return "";
#ifdef _WIN32
    return d + "/ZiGGURAT.lnk";
#else
    return d + "/ziggurat-recomp.desktop";
#endif
}

bool Shortcuts::exists(ShortcutPlace p) {
    std::error_code ec;
    std::string f = pathFor(p);
    return !f.empty() && fs::exists(fs::u8path(f), ec);
}

bool Shortcuts::create(ShortcutPlace p, const std::string& target, const std::string& args, const std::string& workDir,
                       const std::string& iconPath, std::string* err) {
    std::string file = pathFor(p);
    if (file.empty()) { if (err) *err = "unknown folder"; return false; }
    std::error_code ec;
    fs::create_directories(fs::u8path(file).parent_path(), ec);
#ifdef _WIN32
    HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool ok = false;
    IShellLinkW* link = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&link))) {
        auto win = [](std::string p) {
            for (auto& c : p)
                if (c == '/') c = '\\';
            return p;
        };
        link->SetPath(widen(win(target)).c_str());
        link->SetArguments(widen(args).c_str());
        link->SetWorkingDirectory(widen(win(workDir)).c_str());
        link->SetDescription(L"ZiGGURAT (unofficial fan port)");
        // .lnk icons must be .ico/.exe; a .png is ignored by Explorer, so use the launcher's own icon.
        (void)iconPath;
        link->SetIconLocation(widen(target).c_str(), 0);
        IPersistFile* pf = nullptr;
        if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, (void**)&pf))) {
            ok = SUCCEEDED(pf->Save(widen(file).c_str(), TRUE));
            pf->Release();
        }
        link->Release();
    }
    if (SUCCEEDED(init)) CoUninitialize();
    if (!ok && err) *err = "IShellLink failed";
    return ok;
#else
    std::ofstream f(fs::u8path(file), std::ios::trunc);
    f << "[Desktop Entry]\nType=Application\nName=ZiGGURAT\nComment=Unofficial fan port\n"
      << "Exec=\"" << target << "\" " << args << "\nPath=" << workDir << "\nIcon=" << iconPath
      << "\nCategories=Game;ArcadeGame;\nTerminal=false\n";
    f.close();
    fs::permissions(fs::u8path(file), fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                    fs::perm_options::add, ec);
    if (!f && err) *err = "cannot write .desktop file";
    return (bool)f;
#endif
}

bool Shortcuts::remove(ShortcutPlace p) {
    std::error_code ec;
    std::string f = pathFor(p);
    return !f.empty() && fs::remove(fs::u8path(f), ec);
}

}  // namespace zig
