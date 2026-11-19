#include "core/SteamInput.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include "core/Steam.h"

namespace fs = std::filesystem;

namespace zig {
namespace {

// ---- minimal text-VDF parser that remembers byte offsets ------------------------------------
struct Tok {
    enum Kind { Str, Open, Close, End } kind;
    std::string text;   // unescaped string value
    size_t begin = 0, end = 0;   // byte range in the source (incl. quotes)
};

struct Lexer {
    const std::string& s;
    size_t i = 0;
    explicit Lexer(const std::string& src) : s(src) {}
    Tok next() {
        for (;;) {
            while (i < s.size() && std::isspace((unsigned char)s[i])) i++;
            if (i + 1 < s.size() && s[i] == '/' && s[i + 1] == '/') {
                while (i < s.size() && s[i] != '\n') i++;
                continue;
            }
            if (i < s.size() && s[i] == '[') {   // conditional like [$WIN32]: skip
                while (i < s.size() && s[i] != ']') i++;
                if (i < s.size()) i++;
                continue;
            }
            break;
        }
        Tok t{Tok::End, {}, i, i};
        if (i >= s.size()) return t;
        char c = s[i];
        if (c == '{') { t.kind = Tok::Open; t.end = ++i; return t; }
        if (c == '}') { t.kind = Tok::Close; t.end = ++i; return t; }
        t.kind = Tok::Str;
        if (c == '"') {
            i++;
            while (i < s.size() && s[i] != '"') {
                if (s[i] == '\\' && i + 1 < s.size()) { t.text += s[i + 1]; i += 2; continue; }
                t.text += s[i++];
            }
            if (i < s.size()) i++;
        } else {
            while (i < s.size() && !std::isspace((unsigned char)s[i]) && s[i] != '{' && s[i] != '}' && s[i] != '"')
                t.text += s[i++];
        }
        t.end = i;
        return t;
    }
};

struct Node {
    std::string key;
    bool isMap = false;
    Tok keyTok{}, valTok{};          // valTok valid for strings
    size_t open = 0, close = 0;      // brace offsets for maps
    std::vector<Node> kids;
    Node* child(const std::string& k) {
        for (auto& n : kids) {
            if (n.key.size() != k.size()) continue;
            bool eq = true;
            for (size_t j = 0; j < k.size() && eq; j++)
                eq = std::tolower((unsigned char)n.key[j]) == std::tolower((unsigned char)k[j]);
            if (eq) return &n;
        }
        return nullptr;
    }
};

bool parseMap(Lexer& lx, Node& parent, std::string& err, bool top) {
    for (;;) {
        Tok k = lx.next();
        if (k.kind == Tok::End) {
            if (top) return true;
            err = "unexpected end of file";
            return false;
        }
        if (k.kind == Tok::Close) {
            if (top) { err = "unbalanced }"; return false; }
            parent.close = k.begin;
            return true;
        }
        if (k.kind != Tok::Str) { err = "expected key"; return false; }
        Tok v = lx.next();
        Node n;
        n.key = k.text;
        n.keyTok = k;
        if (v.kind == Tok::Open) {
            n.isMap = true;
            n.open = v.begin;
            if (!parseMap(lx, n, err, false)) return false;
        } else if (v.kind == Tok::Str) {
            n.valTok = v;
        } else {
            err = "expected value after \"" + k.text + "\"";
            return false;
        }
        parent.kids.push_back(std::move(n));
    }
}

std::string lineIndent(const std::string& s, size_t pos) {
    size_t ls = s.rfind('\n', pos == 0 ? 0 : pos - 1);
    ls = (ls == std::string::npos) ? 0 : ls + 1;
    size_t e = ls;
    while (e < s.size() && (s[e] == '\t' || s[e] == ' ')) e++;
    return s.substr(ls, e - ls);
}

std::string quoted(const std::string& v) { return "\"" + v + "\""; }

std::string modeValue(const std::string& mode) { return mode == "enabled" ? "2" : mode == "disabled" ? "0" : ""; }

const char* kKey = "UseSteamControllerConfig";

std::string stamp() {
    char b[32];
    time_t t = time(nullptr);
    strftime(b, sizeof b, "%Y%m%d-%H%M%S", localtime(&t));
    return b;
}

}  // namespace

std::string SteamInput::get(const std::string& text, const std::string& shortId) {
    Node root;
    Lexer lx(text);
    std::string err;
    if (!parseMap(lx, root, err, true)) return "default";
    Node* store = root.child("UserLocalConfigStore");
    Node* apps = store ? store->child("apps") : nullptr;
    Node* app = apps ? apps->child(shortId) : nullptr;
    Node* k = app ? app->child(kKey) : nullptr;
    if (!k || k->isMap) return "default";
    if (k->valTok.text == "2") return "enabled";
    if (k->valTok.text == "0") return "disabled";
    return "default";
}

std::string SteamInput::apply(const std::string& text, const std::string& shortId, const std::string& mode,
                              std::string& err) {
    Node root;
    Lexer lx(text);
    if (!parseMap(lx, root, err, true)) return {};
    const std::string nl = text.find("\r\n") != std::string::npos ? "\r\n" : "\n";
    const std::string val = modeValue(mode);
    Node* store = root.child("UserLocalConfigStore");
    if (!store || !store->isMap) { err = "no UserLocalConfigStore"; return {}; }
    Node* apps = store->child("apps");
    Node* app = (apps && apps->isMap) ? apps->child(shortId) : nullptr;
    Node* k = (app && app->isMap) ? app->child(kKey) : nullptr;

    std::string out = text;
    if (val.empty()) {   // "use default": remove the key line if present
        if (!k || k->isMap) return out;
        auto lineStart = [&](size_t pos) {
            size_t ls = out.rfind('\n', pos == 0 ? 0 : pos - 1);
            return (ls == std::string::npos) ? (size_t)0 : ls + 1;
        };
        auto lineEnd = [&](size_t pos) {
            size_t le = out.find('\n', pos);
            return (le == std::string::npos) ? out.size() : le + 1;
        };
        if (app->kids.size() == 1) {
            // Our key is the only content: drop the whole per-app block (this is the block we created),
            // so a set -> default round trip gives back the original bytes. Same for an "apps" map that
            // only contains our block.
            const Node* blk = (apps->kids.size() == 1) ? apps : app;
            size_t from = lineStart(blk->keyTok.begin), to = lineEnd(blk->close);
            out.erase(from, to - from);
            return out;
        }
        size_t ls = lineStart(k->keyTok.begin), le = lineEnd(k->valTok.end);
        out.erase(ls, le - ls);
        return out;
    }
    if (k && !k->isMap) {   // replace the value token
        out.replace(k->valTok.begin, k->valTok.end - k->valTok.begin, quoted(val));
        return out;
    }
    if (app && app->isMap) {   // insert the key before the app block's closing brace
        std::string ind = lineIndent(out, app->close);
        out.insert(app->close, "\t" + quoted(kKey) + "\t\t" + quoted(val) + nl + ind);
        return out;
    }
    if (apps && apps->isMap) {   // insert a new app block
        std::string ind = lineIndent(out, apps->close);
        std::string in1 = ind + "\t";
        std::string blk = "\t" + quoted(shortId) + nl + in1 + "{" + nl + in1 + "\t" + quoted(kKey) + "\t\t" + quoted(val) +
                          nl + in1 + "}" + nl + ind;
        out.insert(apps->close, blk);
        return out;
    }
    // no "apps" map yet: create it inside UserLocalConfigStore
    std::string ind = lineIndent(out, store->close);
    std::string i1 = ind + "\t", i2 = i1 + "\t";
    std::string blk = "\t" + quoted("apps") + nl + i1 + "{" + nl + i1 + "\t" + quoted(shortId) + nl + i2 + "{" + nl + i2 +
                      "\t" + quoted(kKey) + "\t\t" + quoted(val) + nl + i2 + "}" + nl + i1 + "}" + nl + ind;
    out.insert(store->close, blk);
    return out;
}

std::string SteamInput::localConfigPath(const std::string& root, const std::string& acc) {
    return Steam::configDir(root, acc) + "/localconfig.vdf";
}

std::string SteamInput::read(const std::string& root, const std::string& acc, const std::string& shortId) {
    std::ifstream in(fs::u8path(localConfigPath(root, acc)), std::ios::binary);
    if (!in) return "default";
    std::string t((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return get(t, shortId);
}

SteamInputResult SteamInput::write(const std::string& root, const std::string& acc, const std::string& shortId,
                                   const std::string& mode) {
    if (Steam::isRunning()) return {false, "steam.err.running", "Steam is running"};
    std::string path = localConfigPath(root, acc);
    std::string text;
    {
        std::ifstream in(fs::u8path(path), std::ios::binary);
        if (in) text.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }
    if (text.empty()) {
        if (modeValue(mode).empty()) return {true, "extras.si.ok", "nothing to do"};
        text = "\"UserLocalConfigStore\"\n{\n}\n";
    }
    std::string err;
    std::string out = apply(text, shortId, mode, err);
    if (out.empty()) return {false, "extras.si.err", err};
    if (out == text) return {true, "extras.si.ok", "unchanged"};
    std::error_code ec;
    if (fs::exists(fs::u8path(path), ec)) {   // backup next to the file, keep the newest 5
        std::string bak = path + ".ziggurat-backup-" + stamp();
        for (int i = 2; fs::exists(fs::u8path(bak), ec); i++)   // several changes within one second
            bak = path + ".ziggurat-backup-" + stamp() + "-" + std::to_string(i);
        fs::copy_file(fs::u8path(path), fs::u8path(bak), fs::copy_options::none, ec);
        if (ec) return {false, "steam.err.backup", ec.message()};
        std::vector<fs::path> baks;
        for (auto& e : fs::directory_iterator(fs::u8path(path).parent_path(), ec))
            if (e.path().filename().u8string().rfind("localconfig.vdf.ziggurat-backup-", 0) == 0) baks.push_back(e.path());
        std::sort(baks.begin(), baks.end());
        while (baks.size() > 5) { fs::remove(baks.front(), ec); baks.erase(baks.begin()); }
    }
    std::string tmp = path + ".tmp";
    {
        std::ofstream o(fs::u8path(tmp), std::ios::binary | std::ios::trunc);
        o << out;
        if (!o) return {false, "extras.si.err", "write failed"};
    }
    fs::rename(fs::u8path(tmp), fs::u8path(path), ec);
    if (ec) return {false, "extras.si.err", ec.message()};
    SDL_Log("steam input: shortcut %s set to %s", shortId.c_str(), mode.c_str());
    return {true, "extras.si.ok", ""};
}

}  // namespace zig
