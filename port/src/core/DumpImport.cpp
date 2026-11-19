#include "core/DumpImport.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>

#include "core/Manifest.h"
#include "core/Sha256.h"
#include "core/Zip.h"
#include "json.hpp"

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#include "stb_image_write.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace fs = std::filesystem;

namespace zig {

const char* toString(DumpStatus s) {
    switch (s) {
        case DumpStatus::Verified: return "Verified";
        case DumpStatus::UnknownVersion: return "UnknownVersion";
        case DumpStatus::NotZiggurat: return "NotZiggurat";
        case DumpStatus::Incomplete: return "Incomplete";
        case DumpStatus::Modified: return "Modified";
        case DumpStatus::Unreadable: return "Unreadable";
        case DumpStatus::Compressed: return "Compressed";
        case DumpStatus::NotFound: return "NotFound";
    }
    return "?";
}

namespace {

// ------------------------------------------------------------------ helpers
std::string lower(std::string s) {
    for (auto& c : s) c = (char)tolower((unsigned char)c);
    return s;
}
std::string ext(const std::string& p) { return lower(fs::u8path(p).extension().u8string()); }
std::string baseName(const std::string& rel) {
    auto pos = rel.find_last_of('/');
    return pos == std::string::npos ? rel : rel.substr(pos + 1);
}
bool readFile(const fs::path& p, std::vector<uint8_t>& out) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    in.seekg(0, std::ios::end);
    std::streamoff n = in.tellg();
    if (n < 0) return false;
    in.seekg(0);
    out.resize((size_t)n);
    return n == 0 || (bool)in.read((char*)out.data(), n);
}
bool writeFile(const fs::path& p, const void* data, size_t n) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write((const char*)data, (std::streamsize)n);
    return (bool)out;
}

DumpMessage msg(const std::string& id, std::vector<std::string> args, std::string text) {
    return DumpMessage{id, std::move(args), std::move(text)};
}

std::string listSome(const std::vector<std::string>& v, size_t n = 3) {
    std::string s;
    for (size_t i = 0; i < v.size() && i < n; i++) s += (i ? ", " : "") + v[i];
    if (v.size() > n) s += ", ...";
    return s;
}

// ------------------------------------------------------------------ plist
struct PValue {
    enum T { Null, Bool, Int, Real, Str, Data, Arr, Dict } t = Null;
    bool b = false;
    long long i = 0;
    double r = 0;
    std::string s;  // Str (UTF-8) or Data (raw)
    std::vector<PValue> arr;
    std::vector<std::pair<std::string, PValue>> dict;  // insertion order preserved
    const PValue* get(const std::string& k) const {
        for (auto& kv : dict)
            if (kv.first == k) return &kv.second;
        return nullptr;
    }
    std::string str(const std::string& k) const {
        auto* v = get(k);
        return v && v->t == Str ? v->s : std::string();
    }
};

void appendUtf8(std::string& o, uint32_t cp) {
    if (cp < 0x80) o += (char)cp;
    else if (cp < 0x800) { o += (char)(0xC0 | (cp >> 6)); o += (char)(0x80 | (cp & 63)); }
    else if (cp < 0x10000) { o += (char)(0xE0 | (cp >> 12)); o += (char)(0x80 | ((cp >> 6) & 63)); o += (char)(0x80 | (cp & 63)); }
    else { o += (char)(0xF0 | (cp >> 18)); o += (char)(0x80 | ((cp >> 12) & 63)); o += (char)(0x80 | ((cp >> 6) & 63)); o += (char)(0x80 | (cp & 63)); }
}

std::string utf16ToUtf8(const uint8_t* p, size_t units, bool bigEndian) {
    std::string o;
    for (size_t i = 0; i < units; i++) {
        uint32_t u = bigEndian ? (p[2 * i] << 8 | p[2 * i + 1]) : (p[2 * i] | p[2 * i + 1] << 8);
        if (u >= 0xD800 && u < 0xDC00 && i + 1 < units) {
            uint32_t l = bigEndian ? (p[2 * i + 2] << 8 | p[2 * i + 3]) : (p[2 * i + 2] | p[2 * i + 3] << 8);
            if (l >= 0xDC00 && l < 0xE000) { u = 0x10000 + ((u - 0xD800) << 10) + (l - 0xDC00); i++; }
        }
        appendUtf8(o, u);
    }
    return o;
}

class BPlist {
public:
    explicit BPlist(const std::vector<uint8_t>& d) : d_(d) {}
    bool parse(PValue& out) {
        if (d_.size() < 40 || memcmp(d_.data(), "bplist00", 8) != 0) return false;
        const uint8_t* t = d_.data() + d_.size() - 32;
        offSize_ = t[6];
        refSize_ = t[7];
        numObj_ = be(t + 8, 8);
        uint64_t top = be(t + 16, 8);
        tableOff_ = be(t + 24, 8);
        if (!offSize_ || !refSize_ || tableOff_ + numObj_ * offSize_ > d_.size() - 32) return false;
        return obj(top, out, 0);
    }

private:
    uint64_t be(const uint8_t* p, int n) const {
        uint64_t v = 0;
        for (int i = 0; i < n; i++) v = v << 8 | p[i];
        return v;
    }
    bool offsetOf(uint64_t ref, uint64_t& off) const {
        if (ref >= numObj_) return false;
        off = be(&d_[tableOff_ + ref * offSize_], offSize_);
        return off < d_.size();
    }
    bool count(uint64_t& pos, uint8_t low, uint64_t& n) const {
        if (low != 0xF) { n = low; return true; }
        if (pos >= d_.size()) return false;
        uint8_t m = d_[pos++];
        if ((m >> 4) != 1) return false;
        int bytes = 1 << (m & 0xF);
        if (pos + bytes > d_.size()) return false;
        n = be(&d_[pos], bytes);
        pos += bytes;
        return true;
    }
    bool obj(uint64_t ref, PValue& v, int depth) const {
        uint64_t pos;
        if (depth > 32 || !offsetOf(ref, pos)) return false;
        uint8_t m = d_[pos++];
        uint8_t hi = m >> 4, low = m & 0xF;
        uint64_t n = 0;
        switch (hi) {
            case 0x0:
                v.t = (low == 8 || low == 9) ? PValue::Bool : PValue::Null;
                v.b = low == 9;
                return true;
            case 0x1: {
                int bytes = 1 << low;
                if (pos + bytes > d_.size()) return false;
                v.t = PValue::Int;
                v.i = (long long)be(&d_[pos], bytes);
                return true;
            }
            case 0x2: {
                int bytes = 1 << low;
                if (pos + bytes > d_.size()) return false;
                uint64_t raw = be(&d_[pos], bytes);
                v.t = PValue::Real;
                if (bytes == 4) { uint32_t r32 = (uint32_t)raw; float f; memcpy(&f, &r32, 4); v.r = f; }
                else { double dd; memcpy(&dd, &raw, 8); v.r = dd; }
                return true;
            }
            case 0x3: v.t = PValue::Real; return true;  // date (unused)
            case 0x4:
            case 0x5:
                if (!count(pos, low, n) || pos + n > d_.size()) return false;
                v.t = hi == 4 ? PValue::Data : PValue::Str;
                v.s.assign((const char*)&d_[pos], (size_t)n);
                return true;
            case 0x6:
                if (!count(pos, low, n) || pos + n * 2 > d_.size()) return false;
                v.t = PValue::Str;
                v.s = utf16ToUtf8(&d_[pos], (size_t)n, true);
                return true;
            case 0xA:
                if (!count(pos, low, n) || pos + n * refSize_ > d_.size()) return false;
                v.t = PValue::Arr;
                for (uint64_t i = 0; i < n; i++) {
                    PValue e;
                    if (!obj(be(&d_[pos + i * refSize_], refSize_), e, depth + 1)) return false;
                    v.arr.push_back(std::move(e));
                }
                return true;
            case 0xD:
                if (!count(pos, low, n) || pos + 2 * n * refSize_ > d_.size()) return false;
                v.t = PValue::Dict;
                for (uint64_t i = 0; i < n; i++) {
                    PValue k, e;
                    if (!obj(be(&d_[pos + i * refSize_], refSize_), k, depth + 1)) return false;
                    if (!obj(be(&d_[pos + (n + i) * refSize_], refSize_), e, depth + 1)) return false;
                    v.dict.emplace_back(k.s, std::move(e));
                }
                return true;
            default:
                v.t = PValue::Null;  // uid, set: not needed here
                return true;
        }
    }
    const std::vector<uint8_t>& d_;
    int offSize_ = 0, refSize_ = 0;
    uint64_t numObj_ = 0, tableOff_ = 0;
};

std::string xmlUnescape(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] != '&') { o += s[i]; continue; }
        size_t e = s.find(';', i);
        if (e == std::string::npos) { o += s[i]; continue; }
        std::string ent = s.substr(i + 1, e - i - 1);
        if (ent == "amp") o += '&';
        else if (ent == "lt") o += '<';
        else if (ent == "gt") o += '>';
        else if (ent == "quot") o += '"';
        else if (ent == "apos") o += '\'';
        else if (!ent.empty() && ent[0] == '#') appendUtf8(o, (uint32_t)strtoul(ent.c_str() + (ent[1] == 'x' ? 2 : 1), nullptr, ent[1] == 'x' ? 16 : 10));
        else { o += s.substr(i, e - i + 1); }
        i = e;
    }
    return o;
}

// Minimal XML plist reader: top-level <dict> of <key>/<string|true|false|integer|...>.
bool parseXmlPlist(const std::vector<uint8_t>& d, PValue& out) {
    std::string s(d.begin(), d.end());
    size_t p = s.find("<dict>");
    if (p == std::string::npos || s.find("<plist") == std::string::npos) return false;
    p += 6;
    out.t = PValue::Dict;
    int depth = 0;
    std::string key;
    while (p < s.size()) {
        size_t lt = s.find('<', p);
        if (lt == std::string::npos) break;
        size_t gt = s.find('>', lt);
        if (gt == std::string::npos) break;
        std::string tag = s.substr(lt + 1, gt - lt - 1);
        p = gt + 1;
        if (tag == "/dict" && depth == 0) return true;
        if (tag == "dict" || tag == "array") { depth++; continue; }
        if (tag == "/dict" || tag == "/array") { depth--; continue; }
        if (depth != 0) continue;
        auto textUntil = [&](const std::string& close) {
            size_t e = s.find(close, p);
            std::string t = s.substr(p, e == std::string::npos ? 0 : e - p);
            p = e == std::string::npos ? s.size() : e + close.size();
            return xmlUnescape(t);
        };
        if (tag == "key") { key = textUntil("</key>"); continue; }
        PValue v;
        if (tag == "string") { v.t = PValue::Str; v.s = textUntil("</string>"); }
        else if (tag == "true/" || tag == "false/") { v.t = PValue::Bool; v.b = tag == "true/"; }
        else if (tag == "integer") { v.t = PValue::Int; v.i = atoll(textUntil("</integer>").c_str()); }
        else if (tag == "real") { v.t = PValue::Real; v.r = atof(textUntil("</real>").c_str()); }
        else continue;
        out.dict.emplace_back(key, std::move(v));
    }
    return true;
}

// Old-style text .strings: "key" = "value"; (UTF-8 or UTF-16 with BOM).
bool parseTextStrings(const std::vector<uint8_t>& d, PValue& out) {
    std::string s;
    if (d.size() >= 2 && d[0] == 0xFF && d[1] == 0xFE) s = utf16ToUtf8(d.data() + 2, (d.size() - 2) / 2, false);
    else if (d.size() >= 2 && d[0] == 0xFE && d[1] == 0xFF) s = utf16ToUtf8(d.data() + 2, (d.size() - 2) / 2, true);
    else s.assign(d.begin(), d.end());
    out.t = PValue::Dict;
    size_t i = 0;
    auto skipWs = [&] {
        for (;;) {
            while (i < s.size() && isspace((unsigned char)s[i])) i++;
            if (s.compare(i, 2, "/*") == 0) { size_t e = s.find("*/", i + 2); i = e == std::string::npos ? s.size() : e + 2; }
            else if (s.compare(i, 2, "//") == 0) { size_t e = s.find('\n', i); i = e == std::string::npos ? s.size() : e + 1; }
            else break;
        }
    };
    auto quoted = [&](std::string& o) -> bool {
        if (i >= s.size() || s[i] != '"') return false;
        i++;
        o.clear();
        while (i < s.size() && s[i] != '"') {
            if (s[i] == '\\' && i + 1 < s.size()) {
                char e = s[++i];
                if (e == 'n') o += '\n'; else if (e == 't') o += '\t'; else if (e == 'r') o += '\r';
                else if (e == 'U' || e == 'u') { appendUtf8(o, (uint32_t)strtoul(s.substr(i + 1, 4).c_str(), nullptr, 16)); i += 4; }
                else o += e;
                i++;
            } else o += s[i++];
        }
        i++;
        return true;
    };
    while (true) {
        skipWs();
        if (i >= s.size()) break;
        std::string k, v;
        if (!quoted(k)) return false;
        skipWs();
        if (i >= s.size() || s[i] != '=') return false;
        i++;
        skipWs();
        if (!quoted(v)) return false;
        skipWs();
        if (i < s.size() && s[i] == ';') i++;
        PValue pv;
        pv.t = PValue::Str;
        pv.s = v;
        out.dict.emplace_back(k, std::move(pv));
    }
    return true;
}

bool parsePlistAny(const std::vector<uint8_t>& d, PValue& out) {
    if (d.size() >= 8 && memcmp(d.data(), "bplist00", 8) == 0) return BPlist(d).parse(out);
    if (parseXmlPlist(d, out)) return true;
    out = PValue{};
    return false;
}

// ------------------------------------------------------------------ sources
class Source {
public:
    virtual ~Source() = default;
    virtual bool open(DumpMessage& err) = 0;
    virtual std::vector<std::string> list() = 0;                 // paths relative to .app (+ "iTunesArtwork")
    virtual bool has(const std::string& rel) = 0;
    virtual bool read(const std::string& rel, std::vector<uint8_t>& out, std::string& err) = 0;
};

class IpaSource : public Source {
public:
    explicit IpaSource(std::string p) : path_(std::move(p)) {}
    bool open(DumpMessage& err) override {
        if (!zip_.open(path_)) {
            err = msg("dump.unreadable", {path_, zip_.error()}, "Can't read the file: " + zip_.error() + ".");
            return false;
        }
        for (auto& e : zip_.entries()) {
            // Payload/<X>.app/Info.plist (top-level app only)
            const std::string& n = e.name;
            if (n.rfind("Payload/", 0) == 0) {
                size_t a = n.find(".app/", 8);
                if (a != std::string::npos && n.compare(a + 5, std::string::npos, "Info.plist") == 0 &&
                    n.find('/', 8) == a + 4) {
                    prefix_ = n.substr(0, a + 5);
                    break;
                }
            }
        }
        if (prefix_.empty()) {
            err = msg("dump.not_app", {}, "This archive does not contain an iOS app (no Payload/*.app/Info.plist).");
            return false;
        }
        for (auto& e : zip_.entries()) {
            if (e.name.back() == '/') continue;
            if (e.name.rfind(prefix_, 0) == 0) index_[e.name.substr(prefix_.size())] = &e;
            else if (e.name == "iTunesArtwork") index_["iTunesArtwork"] = &e;
        }
        return true;
    }
    std::vector<std::string> list() override {
        std::vector<std::string> v;
        for (auto& kv : index_) v.push_back(kv.first);
        return v;
    }
    bool has(const std::string& rel) override { return index_.count(rel) > 0; }
    bool read(const std::string& rel, std::vector<uint8_t>& out, std::string& err) override {
        auto it = index_.find(rel);
        if (it == index_.end()) { err = "missing"; return false; }
        if (!zip_.read(*it->second, out)) { err = zip_.error(); return false; }
        return true;
    }

private:
    std::string path_, prefix_;
    ZipArchive zip_;
    std::map<std::string, const ZipArchive::Entry*> index_;
};

class DirSource : public Source {
public:
    explicit DirSource(std::string p) : root_(fs::u8path(p)) {}
    bool open(DumpMessage& err) override {
        std::error_code ec;
        if (!fs::is_regular_file(root_ / "Info.plist", ec)) {
            err = msg("dump.not_app", {}, "This folder is not an iOS app (no Info.plist).");
            return false;
        }
        for (auto it = fs::recursive_directory_iterator(root_, fs::directory_options::skip_permission_denied, ec);
             !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (it->is_regular_file(ec)) {
                std::string rel = fs::relative(it->path(), root_, ec).generic_u8string();
                files_[rel] = it->path();
            }
        }
        if (ec) {
            err = msg("dump.unreadable", {root_.u8string(), ec.message()}, "Can't read the folder: " + ec.message() + ".");
            return false;
        }
        // iTunesArtwork lives at the IPA root: <root>/Payload/X.app -> <root>/iTunesArtwork
        for (fs::path cand : {root_.parent_path().parent_path() / "iTunesArtwork", root_.parent_path() / "iTunesArtwork"}) {
            if (fs::is_regular_file(cand, ec)) { files_["iTunesArtwork"] = cand; break; }
        }
        return true;
    }
    std::vector<std::string> list() override {
        std::vector<std::string> v;
        for (auto& kv : files_) v.push_back(kv.first);
        return v;
    }
    bool has(const std::string& rel) override { return files_.count(rel) > 0; }
    bool read(const std::string& rel, std::vector<uint8_t>& out, std::string& err) override {
        auto it = files_.find(rel);
        if (it == files_.end()) { err = "missing"; return false; }
        if (!readFile(it->second, out)) { err = "cannot read " + it->second.u8string(); return false; }
        return true;
    }

private:
    fs::path root_;
    std::map<std::string, fs::path> files_;
};

std::unique_ptr<Source> makeSource(const DumpCandidate& c) {
    if (c.kind == DumpCandidate::Kind::Ipa) return std::make_unique<IpaSource>(c.path);
    return std::make_unique<DirSource>(c.path);
}

// ------------------------------------------------------------------ scanning
bool isArchiveWrapper(const std::string& e) { return e == ".zip" || e == ".rar" || e == ".7z" || e == ".gz" || e == ".tar"; }

bool ipaLooksValid(const fs::path& p) {
    ZipArchive z;
    if (!z.open(p.u8string())) return false;
    for (auto& e : z.entries())
        if (e.name.rfind("Payload/", 0) == 0 && e.name.size() > 19 &&
            e.name.compare(e.name.size() - 11, 11, "/Info.plist") == 0)
            return true;
    return false;
}

void scanDir(const fs::path& dir, int depth, ScanResult& r, std::vector<std::string>& archives) {
    std::error_code ec;
    if (fs::is_regular_file(dir / "Info.plist", ec) && lower(dir.extension().u8string()) == ".app") {
        r.candidates.push_back({DumpCandidate::Kind::AppDir, dir.u8string(), dir.filename().u8string()});
        return;
    }
    if (depth > 3) return;
    std::vector<fs::directory_entry> entries;
    for (auto it = fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
         !ec && it != fs::directory_iterator(); it.increment(ec))
        entries.push_back(*it);
    std::sort(entries.begin(), entries.end(), [](auto& a, auto& b) { return a.path() < b.path(); });
    for (auto& e : entries) {
        std::string x = lower(e.path().extension().u8string());
        if (e.is_regular_file(ec)) {
            if (x == ".ipa") {
                if (ipaLooksValid(e.path()))
                    r.candidates.push_back({DumpCandidate::Kind::Ipa, e.path().u8string(), e.path().filename().u8string()});
            } else if (isArchiveWrapper(x)) {
                archives.push_back(e.path().filename().u8string());
            }
        } else if (e.is_directory(ec)) {
            scanDir(e.path(), depth + 1, r, archives);
        }
    }
}

int candidateScore(const DumpCandidate& c) {
    std::string l = lower(c.label);
    int s = 0;
    if (l.find("zig") != std::string::npos) s += 10;
    if (c.kind == DumpCandidate::Kind::Ipa) s += 1;
    return s;
}

nlohmann::json pvalueToJson(const PValue& v) {
    switch (v.t) {
        case PValue::Bool: return v.b;
        case PValue::Int: return v.i;
        case PValue::Real: return v.r;
        case PValue::Str: return v.s;
        case PValue::Arr: {
            nlohmann::json a = nlohmann::json::array();
            for (auto& e : v.arr) a.push_back(pvalueToJson(e));
            return a;
        }
        case PValue::Dict: {
            nlohmann::json o = nlohmann::json::object();
            for (auto& kv : v.dict) o[kv.first] = pvalueToJson(kv.second);
            return o;
        }
        default: return nullptr;  // Null, Data
    }
}

}  // namespace

// ================================================================== public API

bool binaryPlistToJson(const std::vector<uint8_t>& in, std::string& json) {
    if (in.size() < 8 || memcmp(in.data(), "bplist00", 8) != 0) return false;
    PValue root;
    if (!BPlist(in).parse(root)) return false;
    json = pvalueToJson(root).dump();
    return true;
}

ScanResult scanForDumps(const std::string& path) {
    ScanResult r;
    fs::path p = fs::u8path(path);
    std::error_code ec;
    if (path.empty() || !fs::exists(p, ec)) {
        r.status = DumpStatus::NotFound;
        r.message = msg("dump.not_found", {path}, "Nothing found at \"" + path + "\".");
        return r;
    }
    if (fs::is_regular_file(p, ec)) {
        std::string x = ext(path);
        if (p.filename() == "Info.plist" && lower(p.parent_path().extension().u8string()) == ".app") {
            r.candidates.push_back({DumpCandidate::Kind::AppDir, p.parent_path().u8string(), p.parent_path().filename().u8string()});
            return r;
        }
        std::ifstream probe(p, std::ios::binary);
        if (!probe) {
            r.status = DumpStatus::Unreadable;
            r.message = msg("dump.unreadable", {path, "permission denied"}, "Can't read the file (permission denied or in use).");
            return r;
        }
        bool zipSig = ZipArchive::looksLikeZip(path);
        if (zipSig && ipaLooksValid(p)) {
            r.candidates.push_back({DumpCandidate::Kind::Ipa, path, p.filename().u8string()});
            return r;
        }
        if (isArchiveWrapper(x) || (zipSig && x != ".ipa")) {
            r.status = DumpStatus::Compressed;
            r.message = msg("dump.compressed", {x},
                            "This is a " + (x.empty() ? std::string("compressed") : x) +
                                " archive, not the game: extract it first, or select the .ipa inside.");
            return r;
        }
        if (x == ".ipa" && zipSig) {
            ZipArchive z;
            if (!z.open(path)) {
                r.status = DumpStatus::Unreadable;
                r.message = msg("dump.damaged", {path, z.error()},
                                "This .ipa is damaged or incomplete (" + z.error() + "). Copy it again from the source.");
                return r;
            }
            r.status = DumpStatus::NotZiggurat;
            r.message = msg("dump.not_app", {}, "This archive does not contain an iOS app (no Payload/*.app/Info.plist).");
            return r;
        }
        if (x == ".ipa") {
            r.status = DumpStatus::Unreadable;
            r.message = msg("dump.unreadable", {path, "not a zip"}, "This .ipa is damaged (it is not a valid zip file).");
            return r;
        }
        r.status = DumpStatus::NotZiggurat;
        r.message = msg("dump.not_dump", {p.filename().u8string()},
                        "\"" + p.filename().u8string() + "\" is not a game dump. Select the ZiGGURAT .ipa or its .app folder.");
        return r;
    }
    std::vector<std::string> archives;
    scanDir(p, 0, r, archives);
    std::stable_sort(r.candidates.begin(), r.candidates.end(),
                     [](auto& a, auto& b) { return candidateScore(a) > candidateScore(b); });
    if (r.candidates.empty()) {
        if (!archives.empty()) {
            r.status = DumpStatus::Compressed;
            r.message = msg("dump.compressed_in_folder", {listSome(archives)},
                            "No .ipa found, but there are archives here (" + listSome(archives) +
                                "): extract them first.");
        } else {
            r.status = DumpStatus::NotFound;
            r.message = msg("dump.none_in_folder", {path}, "No ZiGGURAT .ipa or .app found in this folder.");
        }
    }
    return r;
}

DumpInfo inspectDump(const DumpCandidate& c) {
    DumpInfo info;
    auto src = makeSource(c);
    if (!src->open(info.error)) return info;
    std::vector<uint8_t> data;
    std::string err;
    PValue pl;
    if (!src->read("Info.plist", data, err) || !parsePlistAny(data, pl) || pl.t != PValue::Dict) {
        info.error = msg("dump.bad_info_plist", {err}, "The app's Info.plist can't be read.");
        return info;
    }
    info.bundleId = pl.str("CFBundleIdentifier");
    info.version = pl.str("CFBundleShortVersionString");
    if (info.version.empty()) info.version = pl.str("CFBundleVersion");
    info.displayName = pl.str("CFBundleDisplayName");
    if (info.displayName.empty()) info.displayName = pl.str("CFBundleName");
    info.files = src->list();
    info.ok = true;
    return info;
}

VerifyResult verifyDump(const DumpCandidate& c, const DumpProgress& progress) {
    VerifyResult r;
    DumpInfo info = inspectDump(c);
    if (!info.ok) {
        r.status = info.error.id == "dump.unreadable" ? DumpStatus::Unreadable : DumpStatus::NotZiggurat;
        r.message = info.error;
        return r;
    }
    r.bundleId = info.bundleId;
    r.version = info.version;
    if (info.bundleId != Manifest::bundleId()) {
        r.status = DumpStatus::NotZiggurat;
        std::string name = info.displayName.empty() ? info.bundleId : info.displayName + " (" + info.bundleId + ")";
        r.message = msg("dump.not_ziggurat", {name}, "This app is not ZiGGURAT (found: " + name + ").");
        return r;
    }
    auto src = makeSource(c);
    DumpMessage err;
    if (!src->open(err)) {
        r.status = DumpStatus::Unreadable;
        r.message = err;
        return r;
    }
    std::vector<std::string> missing, modified, unreadable;
    r.total = Manifest::count();
    std::vector<uint8_t> data;
    for (int i = 0; i < r.total; i++) {
        const ManifestEntry& e = Manifest::at(i);
        if (progress) progress(i, r.total, e.path);
        FileCheck fc{e.path, FileState::Ok, e.optional};
        std::string rerr;
        if (!src->has(e.path)) {
            fc.state = FileState::Missing;
            if (!e.optional) missing.push_back(baseName(e.path));
            r.missing++;
        } else if (!src->read(e.path, data, rerr)) {
            fc.state = FileState::Missing;
            unreadable.push_back(baseName(e.path) + " (" + rerr + ")");
            r.missing++;
        } else if (data.size() != e.size || Sha256::hexOf(data.data(), data.size()) != e.sha256) {
            fc.state = FileState::Modified;
            modified.push_back(baseName(e.path));
            r.modified++;
        } else {
            r.ok++;
        }
        r.files.push_back(fc);
    }
    if (progress) progress(r.total, r.total, "");
    std::string expected = Manifest::version();
    if (!unreadable.empty()) {
        r.status = DumpStatus::Unreadable;
        r.message = msg("dump.read_errors", {std::to_string(unreadable.size()), listSome(unreadable)},
                        "Some files can't be read: " + listSome(unreadable) + ". The dump may be damaged.");
    } else if (!missing.empty()) {
        r.status = DumpStatus::Incomplete;
        r.message = msg("dump.incomplete", {std::to_string(missing.size()), listSome(missing)},
                        "Files are missing: " + std::to_string(missing.size()) + " (" + listSome(missing) +
                            "). The dump is incomplete.");
    } else if (modified.empty()) {
        r.status = DumpStatus::Verified;
        r.message = msg("dump.verified", {info.version, std::to_string(r.ok), std::to_string(r.total)},
                        "ZiGGURAT " + info.version + " recognised (" + std::to_string(r.ok) + "/" +
                            std::to_string(r.total) + " files OK).");
    } else if (info.version == expected) {
        r.status = DumpStatus::Modified;
        r.message = msg("dump.modified", {std::to_string(modified.size()), listSome(modified)},
                        "Files were modified: " + std::to_string(modified.size()) + " (" + listSome(modified) +
                            "). Use an unmodified dump.");
    } else {
        r.status = DumpStatus::UnknownVersion;
        r.message = msg("dump.unknown_version", {info.version, expected, std::to_string(modified.size())},
                        "ZiGGURAT " + info.version + " is not a known version (expected " + expected + "); " +
                            std::to_string(modified.size()) +
                            " files differ. You can import it anyway (unverified).");
    }
    return r;
}

// ------------------------------------------------------------------ conversions

bool convertCgbiPng(const std::vector<uint8_t>& in, std::vector<uint8_t>& out, std::string* err) {
    auto fail = [&](const char* m) { if (err) *err = m; return false; };
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 13, 10, 26, 10};
    if (in.size() < 8 || memcmp(in.data(), sig, 8) != 0) return fail("not a PNG");
    bool cgbi = false;
    uint32_t w = 0, h = 0;
    uint8_t depth = 0, ctype = 0, interlace = 0;
    std::vector<uint8_t> idat;
    size_t o = 8;
    while (o + 12 <= in.size()) {
        uint32_t len = (uint32_t)in[o] << 24 | in[o + 1] << 16 | in[o + 2] << 8 | in[o + 3];
        if (o + 12 + len > in.size()) return fail("truncated PNG chunk");
        const uint8_t* t = &in[o + 4];
        const uint8_t* d = &in[o + 8];
        if (!memcmp(t, "CgBI", 4)) cgbi = true;
        else if (!memcmp(t, "IHDR", 4) && len >= 13) {
            w = (uint32_t)d[0] << 24 | d[1] << 16 | d[2] << 8 | d[3];
            h = (uint32_t)d[4] << 24 | d[5] << 16 | d[6] << 8 | d[7];
            depth = d[8]; ctype = d[9]; interlace = d[12];
        } else if (!memcmp(t, "IDAT", 4)) idat.insert(idat.end(), d, d + len);
        o += 12 + len;
    }
    if (!cgbi) { out = in; return true; }
    if (depth != 8 || ctype != 6 || interlace != 0 || !w || !h || w > 16384 || h > 16384)
        return fail("unsupported CgBI PNG format");
    const size_t stride = (size_t)w * 4;
    std::vector<uint8_t> raw;
    if (!inflateRaw(idat.data(), idat.size(), raw, (stride + 1) * h) || raw.size() < (stride + 1) * h)
        return fail("CgBI inflate failed");
    std::vector<uint8_t> px((size_t)w * h * 4);
    std::vector<uint8_t> prev(stride, 0), line(stride);
    for (uint32_t y = 0; y < h; y++) {
        const uint8_t* src = &raw[y * (stride + 1)];
        uint8_t f = src[0];
        memcpy(line.data(), src + 1, stride);
        for (size_t i = 0; i < stride; i++) {
            int a = i >= 4 ? line[i - 4] : 0;
            int up = prev[i];
            int c = i >= 4 ? prev[i - 4] : 0;
            switch (f) {
                case 1: line[i] = (uint8_t)(line[i] + a); break;
                case 2: line[i] = (uint8_t)(line[i] + up); break;
                case 3: line[i] = (uint8_t)(line[i] + ((a + up) >> 1)); break;
                case 4: {
                    int p = a + up - c, pa = abs(p - a), pb = abs(p - up), pc = abs(p - c);
                    line[i] = (uint8_t)(line[i] + ((pa <= pb && pa <= pc) ? a : (pb <= pc ? up : c)));
                    break;
                }
                default: break;
            }
        }
        prev = line;
        uint8_t* dst = &px[y * stride];
        for (size_t i = 0; i < stride; i += 4) {
            int B = line[i], G = line[i + 1], R = line[i + 2], A = line[i + 3];
            if (A && A != 255) {  // un-premultiply exactly like tools/cgbi2png.py
                R = std::min(255, R * 255 / A);
                G = std::min(255, G * 255 / A);
                B = std::min(255, B * 255 / A);
            }
            dst[i] = (uint8_t)R; dst[i + 1] = (uint8_t)G; dst[i + 2] = (uint8_t)B; dst[i + 3] = (uint8_t)A;
        }
    }
    out.clear();
    auto sink = [](void* ctx, void* data, int size) {
        auto* v = (std::vector<uint8_t>*)ctx;
        v->insert(v->end(), (uint8_t*)data, (uint8_t*)data + size);
    };
    if (!stbi_write_png_to_func(sink, &out, (int)w, (int)h, 4, px.data(), (int)stride)) return fail("PNG encode failed");
    return true;
}

bool stringsToJson(const std::vector<uint8_t>& in, std::string& json, std::string* err) {
    PValue pl;
    bool ok = parsePlistAny(in, pl) || parseTextStrings(in, pl);
    if (!ok || pl.t != PValue::Dict) {
        if (err) *err = "not a strings table";
        return false;
    }
    nlohmann::ordered_json j = nlohmann::ordered_json::object();
    for (auto& kv : pl.dict) j[kv.first] = kv.second.t == PValue::Str ? kv.second.s : std::string();
    // Same layout as tools/extract_assets.py: json.dump(indent=1, ensure_ascii=False)
    json = j.dump(1, ' ', false, nlohmann::ordered_json::error_handler_t::replace);
    return true;
}

// ------------------------------------------------------------------ import

ImportResult importDump(const DumpCandidate& c, const std::string& destAssetsDir, const DumpProgress& progress,
                        const std::atomic<bool>* cancel, bool allowUnverified) {
    auto t0 = std::chrono::steady_clock::now();
    ImportResult r;
    r.assetsDir = destAssetsDir;
    DumpInfo info = inspectDump(c);
    if (!info.ok) { r.message = info.error; return r; }
    if (info.bundleId != Manifest::bundleId()) {
        r.message = msg("dump.not_ziggurat", {info.bundleId}, "This app is not ZiGGURAT (found: " + info.bundleId + ").");
        return r;
    }
    auto src = makeSource(c);
    DumpMessage err;
    if (!src->open(err)) { r.message = err; return r; }

    fs::path dest = fs::u8path(destAssetsDir);
    fs::path tmp = fs::u8path(destAssetsDir + ".tmp");
    std::error_code ec;
    fs::path parent = dest.parent_path().empty() ? fs::current_path(ec) : dest.parent_path();
    fs::create_directories(parent, ec);
    auto sp = fs::space(parent, ec);
    if (!ec && sp.available < kImportSpaceNeeded) {
        r.message = msg("import.no_space", {std::to_string(kImportSpaceNeeded / (1024 * 1024))},
                        "Not enough disk space (needs " + std::to_string(kImportSpaceNeeded / (1024 * 1024)) + " MB).");
        return r;
    }
    fs::remove_all(tmp, ec);
    if (!fs::create_directories(tmp / "strings", ec) && ec) {
        r.message = msg("import.write_failed", {tmp.u8string(), ec.message()}, "Can't create " + tmp.u8string() + ": " + ec.message());
        return r;
    }
    auto abort = [&](DumpMessage m) {
        std::error_code e2;
        fs::remove_all(tmp, e2);
        r.ok = false;
        r.message = std::move(m);
        return r;
    };

    const int total = Manifest::count();
    std::vector<uint8_t> data, conv;
    for (int i = 0; i < total; i++) {
        const ManifestEntry& e = Manifest::at(i);
        if (cancel && cancel->load()) return abort(msg("import.cancelled", {}, "Import cancelled."));
        if (progress) progress(i, total, e.path);
        std::string rerr;
        if (!src->has(e.path)) {
            if (e.optional) continue;
            return abort(msg("dump.incomplete", {"1", baseName(e.path)}, "Files are missing: 1 (" + baseName(e.path) + "). The dump is incomplete."));
        }
        if (!src->read(e.path, data, rerr))
            return abort(msg("dump.read_errors", {"1", baseName(e.path)}, "Can't read " + baseName(e.path) + ": " + rerr));
        if (!allowUnverified && (data.size() != e.size || Sha256::hexOf(data.data(), data.size()) != e.sha256))
            return abort(msg("dump.modified", {"1", baseName(e.path)}, "File was modified: " + baseName(e.path) + ". Use an unmodified dump."));
        std::string rel = e.path, base = baseName(rel), x = ext(base);
        fs::path outPath;
        const std::vector<uint8_t>* payload = &data;
        std::string jsonText;
        if (base == "Localizable.strings") {
            std::string lang = rel.substr(0, rel.find('/'));
            if (lang.size() > 6 && lang.compare(lang.size() - 6, 6, ".lproj") == 0) lang.resize(lang.size() - 6);
            if (!stringsToJson(data, jsonText, &rerr))
                return abort(msg("import.convert_failed", {rel, rerr}, "Can't convert " + rel + ": " + rerr));
            outPath = tmp / "strings" / fs::u8path(lang + ".json");
            if (!writeFile(outPath, jsonText.data(), jsonText.size()))
                return abort(msg("import.write_failed", {outPath.u8string(), "write"}, "Can't write " + outPath.u8string()));
            r.strings++;
            r.bytesWritten += jsonText.size();
            continue;
        } else if (x == ".png") {
            if (!convertCgbiPng(data, conv, &rerr))
                return abort(msg("import.convert_failed", {rel, rerr}, "Can't convert " + rel + ": " + rerr));
            payload = &conv;
            outPath = tmp / fs::u8path(base);
            r.pngs++;
        } else if (x == ".wav" || x == ".mp3") {
            outPath = tmp / fs::u8path(base);
            r.audio++;
        } else if (x == ".ttf") {
            outPath = tmp / fs::u8path(base);
            r.fonts++;
        } else if (rel == "iTunesArtwork") {
            outPath = tmp / "iTunesArtwork.jpg";
            r.other++;
        } else {
            outPath = tmp / fs::u8path(base);
            r.other++;
        }
        if (!writeFile(outPath, payload->data(), payload->size()))
            return abort(msg("import.write_failed", {outPath.u8string(), "write"}, "Can't write " + outPath.u8string()));
        r.bytesWritten += payload->size();
    }
    if (progress) progress(total, total, "");
    // Atomic-ish swap: dest -> dest.old, tmp -> dest, remove dest.old.
    fs::path old = fs::u8path(destAssetsDir + ".old");
    fs::remove_all(old, ec);
    if (fs::exists(dest, ec)) {
        fs::rename(dest, old, ec);
        if (ec) return abort(msg("import.write_failed", {dest.u8string(), ec.message()}, "Can't replace " + dest.u8string() + ": " + ec.message()));
    }
    fs::rename(tmp, dest, ec);
    if (ec) {
        std::error_code e2;
        if (fs::exists(old, e2)) fs::rename(old, dest, e2);
        return abort(msg("import.write_failed", {dest.u8string(), ec.message()}, "Can't move files into " + dest.u8string() + ": " + ec.message()));
    }
    fs::remove_all(old, ec);
    r.ok = true;
    r.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    r.message = msg("import.ok", {std::to_string(r.pngs + r.audio + r.fonts + r.strings + r.other)},
                    "Imported " + std::to_string(r.pngs + r.audio + r.fonts + r.strings + r.other) + " files.");
    return r;
}

}  // namespace zig
