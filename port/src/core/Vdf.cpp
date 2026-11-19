#include "core/Vdf.h"

#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace zig::vdf {
namespace {

bool ieq(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    return true;
}

struct Reader {
    const std::vector<uint8_t>& d;
    size_t p = 0;
    std::string err;
    bool cstr(std::string& out) {
        size_t e = p;
        while (e < d.size() && d[e] != 0) e++;
        if (e >= d.size()) { err = "unterminated string"; return false; }
        out.assign((const char*)d.data() + p, e - p);
        p = e + 1;
        return true;
    }
    bool wstr(std::string& out) {  // UTF-16LE, kept as raw bytes (incl. terminator excluded)
        size_t e = p;
        while (e + 1 < d.size() && !(d[e] == 0 && d[e + 1] == 0)) e += 2;
        if (e + 1 >= d.size()) { err = "unterminated wstring"; return false; }
        out.assign((const char*)d.data() + p, e - p);
        p = e + 2;
        return true;
    }
    bool u32(uint32_t& v) {
        if (p + 4 > d.size()) { err = "truncated int"; return false; }
        v = d[p] | (d[p + 1] << 8) | (d[p + 2] << 16) | ((uint32_t)d[p + 3] << 24);
        p += 4;
        return true;
    }
    bool u64(uint64_t& v) {
        uint32_t lo, hi;
        if (!u32(lo) || !u32(hi)) return false;
        v = lo | ((uint64_t)hi << 32);
        return true;
    }
    bool map(BNode& m, int depth) {
        if (depth > 64) { err = "nesting too deep"; return false; }
        while (true) {
            if (p >= d.size()) { err = "missing end marker"; return false; }
            uint8_t t = d[p++];
            if (t == (uint8_t)BType::End) return true;
            BNode n;
            n.type = (BType)t;
            if (!cstr(n.key)) return false;
            switch (n.type) {
                case BType::Map: if (!map(n, depth + 1)) return false; break;
                case BType::String: if (!cstr(n.str)) return false; break;
                case BType::WString: if (!wstr(n.str)) return false; break;
                case BType::Int32: case BType::Float32: case BType::Ptr: case BType::Color:
                    if (!u32(n.u32)) return false;
                    break;
                case BType::UInt64: case BType::Int64:
                    if (!u64(n.u64)) return false;
                    break;
                default: err = "unknown type byte " + std::to_string(t); return false;
            }
            m.kids.push_back(std::move(n));
        }
    }
};

void put32(std::vector<uint8_t>& o, uint32_t v) {
    for (int i = 0; i < 4; i++) o.push_back((uint8_t)(v >> (8 * i)));
}

void writeMap(const BNode& m, std::vector<uint8_t>& o) {
    for (const BNode& n : m.kids) {
        o.push_back((uint8_t)n.type);
        o.insert(o.end(), n.key.begin(), n.key.end());
        o.push_back(0);
        switch (n.type) {
            case BType::Map: writeMap(n, o); break;
            case BType::String:
                o.insert(o.end(), n.str.begin(), n.str.end());
                o.push_back(0);
                break;
            case BType::WString:
                o.insert(o.end(), n.str.begin(), n.str.end());
                o.push_back(0);
                o.push_back(0);
                break;
            case BType::Int32: case BType::Float32: case BType::Ptr: case BType::Color: put32(o, n.u32); break;
            case BType::UInt64: case BType::Int64:
                put32(o, (uint32_t)n.u64);
                put32(o, (uint32_t)(n.u64 >> 32));
                break;
            default: break;
        }
    }
    o.push_back((uint8_t)BType::End);
}

}  // namespace

BNode* BNode::find(const std::string& k) {
    for (auto& c : kids)
        if (ieq(c.key, k)) return &c;
    return nullptr;
}
const BNode* BNode::find(const std::string& k) const {
    for (auto& c : kids)
        if (ieq(c.key, k)) return &c;
    return nullptr;
}
std::string BNode::getStr(const std::string& k, const std::string& def) const {
    const BNode* n = find(k);
    return (n && n->type == BType::String) ? n->str : def;
}
int32_t BNode::getInt(const std::string& k, int32_t def) const {
    const BNode* n = find(k);
    return (n && n->type == BType::Int32) ? (int32_t)n->u32 : def;
}
BNode& BNode::setStr(const std::string& k, const std::string& v) {
    BNode* n = find(k);
    if (!n) { kids.push_back(str_(k, v)); return kids.back(); }
    n->type = BType::String;
    n->str = v;
    n->kids.clear();
    return *n;
}
BNode& BNode::setInt(const std::string& k, int32_t v) {
    BNode* n = find(k);
    if (!n) { kids.push_back(int_(k, v)); return kids.back(); }
    n->type = BType::Int32;
    n->u32 = (uint32_t)v;
    n->kids.clear();
    return *n;
}
BNode& BNode::setMap(const std::string& k) {
    BNode* n = find(k);
    if (!n) {
        BNode m;
        m.type = BType::Map;
        m.key = k;
        kids.push_back(m);
        return kids.back();
    }
    n->type = BType::Map;
    return *n;
}
BNode BNode::str_(const std::string& k, const std::string& v) {
    BNode n;
    n.type = BType::String;
    n.key = k;
    n.str = v;
    return n;
}
BNode BNode::int_(const std::string& k, int32_t v) {
    BNode n;
    n.type = BType::Int32;
    n.key = k;
    n.u32 = (uint32_t)v;
    return n;
}

bool parseBinary(const std::vector<uint8_t>& data, BNode& root, std::string* err) {
    root = BNode{};
    root.type = BType::Map;
    Reader r{data};
    bool ok = r.map(root, 0);
    if (ok && r.p != data.size()) { ok = false; r.err = "trailing bytes after end marker"; }
    if (!ok && err) *err = r.err + " at offset " + std::to_string(r.p);
    return ok;
}

std::vector<uint8_t> writeBinary(const BNode& root) {
    std::vector<uint8_t> o;
    writeMap(root, o);
    return o;
}

BNode emptyShortcuts() {
    BNode root;
    root.type = BType::Map;
    root.setMap("shortcuts");
    return root;
}

// ---------------------------------------------------------------- text
const TNode* TNode::find(const std::string& k) const {
    for (auto& c : kids)
        if (ieq(c.key, k)) return &c;
    return nullptr;
}
std::string TNode::get(const std::string& k, const std::string& def) const {
    const TNode* n = find(k);
    return (n && !n->isMap) ? n->value : def;
}

namespace {
struct TReader {
    const std::string& s;
    size_t p = 0;
    std::string err;
    void ws() {
        while (p < s.size()) {
            if (std::isspace((unsigned char)s[p])) p++;
            else if (s.compare(p, 2, "//") == 0) { while (p < s.size() && s[p] != '\n') p++; }
            else break;
        }
    }
    bool token(std::string& out, bool& isBrace) {
        ws();
        isBrace = false;
        if (p >= s.size()) return false;
        if (s[p] == '{' || s[p] == '}') { out = s[p++]; isBrace = true; return true; }
        out.clear();
        if (s[p] == '"') {
            p++;
            while (p < s.size() && s[p] != '"') {
                if (s[p] == '\\' && p + 1 < s.size()) {
                    char e = s[p + 1];
                    out += e == 'n' ? '\n' : e == 't' ? '\t' : e;
                    p += 2;
                } else out += s[p++];
            }
            if (p >= s.size()) { err = "unterminated quote"; return false; }
            p++;
            return true;
        }
        while (p < s.size() && !std::isspace((unsigned char)s[p]) && s[p] != '{' && s[p] != '}') out += s[p++];
        return true;
    }
    bool map(TNode& m, int depth) {
        if (depth > 64) { err = "too deep"; return false; }
        while (true) {
            std::string k;
            bool br;
            if (!token(k, br)) return depth == 0;  // EOF only valid at top level
            if (br && k == "}") return depth > 0;
            if (br) { err = "unexpected {"; return false; }
            std::string v;
            if (!token(v, br)) { err = "missing value"; return false; }
            TNode n;
            n.key = k;
            if (br && v == "{") {
                n.isMap = true;
                if (!map(n, depth + 1)) return false;
            } else if (br) { err = "unexpected }"; return false; }
            else n.value = v;
            m.kids.push_back(std::move(n));
        }
    }
};
}  // namespace

bool parseText(const std::string& text, TNode& root, std::string* err) {
    root = TNode{};
    root.isMap = true;
    TReader r{text};
    bool ok = r.map(root, 0);
    if (!ok && err) *err = r.err;
    return ok;
}

bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream in(fs::u8path(path), std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), {});
    return true;
}

}  // namespace zig::vdf
