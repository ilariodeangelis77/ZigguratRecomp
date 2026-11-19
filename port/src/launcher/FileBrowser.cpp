#include "launcher/FileBrowser.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <filesystem>

#include "core/Paths.h"
#include "launcher/Strings.h"
#include "launcher/ui/ZigUI.h"

namespace fs = std::filesystem;
using namespace zui;

namespace zl {
namespace {
std::string lower(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}
std::string userDir(SDL_Folder f) {
    const char* p = SDL_GetUserFolder(f);
    return p ? p : "";
}
}  // namespace

void FileBrowser::openFiles(const std::string& startDir, const std::vector<std::string>& exts, const char* titleId,
                            bool allowFolder) {
    generic_ = true;
    exts_ = exts;
    titleId_ = titleId ? titleId : "fb.title";
    allowFolder_ = allowFolder;
    open_ = true;
    result_.clear();
    std::string d = startDir;
    if (d.empty() || !zig::dirExists(d)) d = userDir(SDL_FOLDER_DOCUMENTS);
    if (d.empty() || !zig::dirExists(d)) d = userDir(SDL_FOLDER_HOME);
    load(d);
}

void FileBrowser::open(const std::string& startDir) {
    generic_ = false;
    exts_.clear();
    titleId_ = "fb.title";
    allowFolder_ = true;
    open_ = true;
    result_.clear();
    std::string d = startDir;
    if (d.empty() || !zig::dirExists(d)) d = userDir(SDL_FOLDER_DOWNLOADS);
    if (d.empty() || !zig::dirExists(d)) d = userDir(SDL_FOLDER_HOME);
    load(d);
}

void FileBrowser::load(const std::string& dir) {
    dir_ = dir;
    entries_.clear();
    sel_ = 0;
    std::error_code ec;
    fs::path p = fs::u8path(dir);
    if (!dir.empty() && p.has_parent_path() && p.parent_path() != p)
        entries_.push_back({lz::T("fb.up"), p.parent_path().u8string(), true, false});
    std::vector<Entry> dirs, files;
    if (!dir.empty()) {
        for (auto& e : fs::directory_iterator(p, fs::directory_options::skip_permission_denied, ec)) {
            std::string n = e.path().filename().u8string();
            if (n.empty() || n[0] == '.' || n[0] == '$') continue;
            std::error_code ec2;
            bool isDir = e.is_directory(ec2);
            std::string ln = lower(n);
            bool isApp = !generic_ && isDir && ln.size() > 4 && ln.compare(ln.size() - 4, 4, ".app") == 0;
            bool isIpa = !isDir && ln.size() > 4 && ln.compare(ln.size() - 4, 4, ".ipa") == 0;
            if (generic_) {   // L7: any file matching exts_ (or every file when exts_ is empty)
                bool match = exts_.empty();
                for (auto& x : exts_)
                    if (ln.size() >= x.size() && ln.compare(ln.size() - x.size(), x.size(), x) == 0) match = true;
                isIpa = !isDir && match;
            }
            if (isApp) files.push_back({n, e.path().u8string(), false, false});
            else if (isDir) dirs.push_back({n + "/", e.path().u8string(), true, false});
            else if (isIpa) files.push_back({n, e.path().u8string(), false, false});
        }
    }
    auto byName = [](const Entry& a, const Entry& b) { return lower(a.name) < lower(b.name); };
    std::sort(dirs.begin(), dirs.end(), byName);
    std::sort(files.begin(), files.end(), byName);
    // game candidates first, then folders
    entries_.insert(entries_.end(), files.begin(), files.end());
    entries_.insert(entries_.end(), dirs.begin(), dirs.end());
    // places
    struct P { const char* id; SDL_Folder f; };
    for (P pl : {P{"fb.home", SDL_FOLDER_HOME}, P{"fb.desktop", SDL_FOLDER_DESKTOP}, P{"fb.downloads", SDL_FOLDER_DOWNLOADS},
                 P{"fb.documents", SDL_FOLDER_DOCUMENTS}}) {
        std::string d = userDir(pl.f);
        if (!d.empty()) entries_.push_back({"> " + lz::T(pl.id), d, true, true});
    }
#if defined(_WIN32)
    for (char c = 'C'; c <= 'Z'; c++) {
        std::string d = std::string(1, c) + ":/";
        std::error_code e2;
        if (fs::exists(fs::u8path(d), e2)) entries_.push_back({"> " + d, d, true, true});
    }
#else
    entries_.push_back({"> /", "/", true, true});
#endif
    labels_.clear();
    for (auto& e : entries_) labels_.push_back(e.name);
}

bool FileBrowser::frame() {
    if (!open_) return false;
    UI& u = ui();
    Rect p{8, 22, 464, 246};
    u.panel(p, FrameStyle::Big);
    Rect c = theme().frameContent(p, FrameStyle::Big);
    theme().text(lz::T(titleId_.c_str()), c.x, c.y, Theme::kSmall, pal::emphasis);
    std::string shown = dir_;
    while (theme().font().width(shown, Theme::kSmall) > c.w && shown.size() > 4) shown = "..." + shown.substr(4);
    theme().text(shown, c.x, c.y + 11, Theme::kSmall, pal::textDim);
    Rect lr{c.x, c.y + 24, c.w, c.h - 50};
    if (u.focused().empty() || u.focused() == "fb.none") u.focus("fb.list");
    bool picked = false;
    if (u.list("fb.list", lr, labels_, sel_, lz::T("fb.help"))) {
        if (sel_ >= 0 && sel_ < (int)entries_.size()) {
            const Entry& e = entries_[sel_];
            if (e.dir) load(e.path);
            else { result_ = e.path; picked = true; }
        }
    }
    int by = c.y + c.h - 22;
    if (allowFolder_ && (u.button("fb.use", {c.x, by, 150, 22}, lz::T("fb.use_folder"), lz::T("fb.help")) ||
                         (u.isFocused("fb.list") && input().hit(Act::Y)))) {
        result_ = dir_;
        picked = true;
    }
    if (u.button("fb.cancel", {c.x + c.w - 110, by, 110, 22}, lz::T("btn.cancel")) || u.back()) open_ = false;
    if (picked) open_ = false;
    return picked;
}

}  // namespace zl
