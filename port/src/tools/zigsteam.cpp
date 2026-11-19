// zigsteam: test/maintenance CLI for zigcore Steam integration (L6).
//   zigsteam detect                                   (read-only)
//   zigsteam validate-appids                          (read-only: recompute appids of existing entries)
//   zigsteam roundtrip <shortcuts.vdf>                (read-only: parse -> write must be byte-identical)
//   zigsteam selftest                                 (synthetic VDF/crc tests, no files)
//   zigsteam dump <shortcuts.vdf> [--values]          (structure; values hidden unless --values)
//   zigsteam gen-art <assetsDir> <outDir>
//   zigsteam add|remove|undo|status --steam-root <sandbox> [--account N] --exe <launcher> --assets <dir> [--fake-running]
//   zigsteam shortcut create|remove|status --dir <sandbox> --exe <launcher>
// Write commands REQUIRE --steam-root / --dir: this tool never modifies the real Steam install.
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "core/ArtworkGen.h"
#include "core/Shortcuts.h"
#include "core/Steam.h"
#include "core/Vdf.h"

using namespace zig;
namespace fs = std::filesystem;

static int fails = 0;
static void check(bool c, const char* what) {
    printf("  [%s] %s\n", c ? "ok" : "FAIL", what);
    if (!c) fails++;
}

static const char* typeName(vdf::BType t) {
    switch (t) {
        case vdf::BType::Map: return "map";
        case vdf::BType::String: return "string";
        case vdf::BType::Int32: return "int32";
        case vdf::BType::UInt64: return "uint64";
        case vdf::BType::Float32: return "float";
        default: return "other";
    }
}

static void dumpNode(const vdf::BNode& n, int depth, bool values) {
    for (const auto& k : n.kids) {
        printf("%*s%s (%s)", depth * 2, "", k.key.c_str(), typeName(k.type));
        if (values && k.type == vdf::BType::String) printf(" = \"%s\"", k.str.c_str());
        if (values && k.type == vdf::BType::Int32) printf(" = %d", (int)(int32_t)k.u32);
        printf("\n");
        if (k.type == vdf::BType::Map) dumpNode(k, depth + 1, values);
    }
}

static bool roundtrip(const std::string& file, std::string* why) {
    std::vector<uint8_t> d;
    if (!vdf::readFile(file, d)) { *why = "cannot read"; return false; }
    vdf::BNode r;
    if (!vdf::parseBinary(d, r, why)) return false;
    auto w = vdf::writeBinary(r);
    if (w != d) { *why = "bytes differ (" + std::to_string(w.size()) + " vs " + std::to_string(d.size()) + ")"; return false; }
    return true;
}

static int selftest() {
    printf("selftest:\n");
    // crc32 known vector ("123456789" -> CBF43926)
    check(Steam::crc32("123456789") == 0xCBF43926u, "crc32('123456789') == 0xCBF43926");
    // id derivation shape
    ShortcutIds ids = Steam::idsFor("\"C:\\Games\\x.exe\"", "X");
    check((ids.top & 0x80000000u) != 0, "top bit set");
    check((uint32_t)ids.appid == ids.top, "appid == signed(top)");
    check(ids.bigPicture == (((uint64_t)ids.top << 32) | 0x02000000u), "big-picture id");
    // empty file
    auto e = vdf::writeBinary(vdf::emptyShortcuts());
    const uint8_t expect[] = {0, 's', 'h', 'o', 'r', 't', 'c', 'u', 't', 's', 0, 8, 8};
    check(e.size() == 13 && memcmp(e.data(), expect, 13) == 0, "empty shortcuts.vdf = 13 bytes");
    // synthetic entry with all types + nested tags + unknown fields round-trips
    vdf::BNode root = vdf::emptyShortcuts();
    auto& sc = *root.find("shortcuts");
    vdf::BNode ent;
    ent.type = vdf::BType::Map;
    ent.key = "0";
    ent.setInt("appid", -123456);
    ent.setStr("AppName", "Some Game");
    ent.setStr("Exe", "\"C:\\a b\\g.exe\"");
    ent.setStr("UnknownFutureField", "keep me");
    vdf::BNode u64;
    u64.type = vdf::BType::UInt64;
    u64.key = "BigNumber";
    u64.u64 = 0x0123456789ABCDEFull;
    ent.kids.push_back(u64);
    auto& tags = ent.setMap("tags");
    tags.setStr("0", "Action");
    tags.setStr("1", "Ünïcødé");
    sc.kids.push_back(ent);
    auto bytes = vdf::writeBinary(root);
    vdf::BNode back;
    std::string err;
    check(vdf::parseBinary(bytes, back, &err), "synthetic parses");
    check(vdf::writeBinary(back) == bytes, "synthetic round-trips byte-identically");
    const vdf::BNode* b0 = back.find("shortcuts")->find("0");
    check(b0 && b0->getStr("appname") == "Some Game", "case-insensitive lookup (appname)");
    check(b0 && b0->getInt("APPID") == -123456, "signed int32 kept");
    check(b0 && b0->find("BigNumber") && b0->find("BigNumber")->u64 == 0x0123456789ABCDEFull, "uint64 kept");
    // malformed inputs are rejected
    std::vector<uint8_t> trunc(bytes.begin(), bytes.end() - 3);
    check(!vdf::parseBinary(trunc, back), "truncated file rejected");
    std::vector<uint8_t> bad = bytes;
    bad[0] = 0x42;
    check(!vdf::parseBinary(bad, back), "unknown type byte rejected");
    // text VDF
    vdf::TNode t;
    check(vdf::parseText("\"users\" { \"76561197960287930\" { \"PersonaName\" \"x\" \"MostRecent\" \"1\" } }", t),
          "text vdf parses");
    check(t.find("users") && t.find("users")->kids.size() == 1, "text vdf structure");
    printf("selftest: %s (%d failures)\n", fails ? "FAILED" : "passed", fails);
    return fails ? 1 : 0;
}

static std::string arg(int argc, char** argv, const char* name, const std::string& def = "") {
    for (int i = 1; i + 1 < argc; i++)
        if (!strcmp(argv[i], name)) return argv[i + 1];
    return def;
}
static bool flag(int argc, char** argv, const char* name) {
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], name)) return true;
    return false;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("usage: see header of src/tools/zigsteam.cpp\n");
        return 2;
    }
    std::string cmd = argv[1];
    if (cmd == "selftest") return selftest();
    if (cmd == "roundtrip" && argc >= 3) {
        std::string why;
        bool ok = roundtrip(argv[2], &why);
        printf("roundtrip: %s %s\n", ok ? "identical" : "FAILED", why.c_str());
        return ok ? 0 : 1;
    }
    if (cmd == "dump" && argc >= 3) {
        std::vector<uint8_t> d;
        vdf::BNode r;
        std::string err;
        if (!vdf::readFile(argv[2], d) || !vdf::parseBinary(d, r, &err)) { printf("cannot parse: %s\n", err.c_str()); return 1; }
        dumpNode(r, 0, flag(argc, argv, "--values"));
        return 0;
    }
    if (cmd == "gen-art" && argc >= 4) {
        auto r = ArtworkGen::generate(argv[2], argv[3]);
        printf("gen-art: %s %s (%d files)\n", r.ok ? "ok" : "FAILED", r.error.c_str(), (int)r.files.size());
        return r.ok ? 0 : 1;
    }
    std::string root = arg(argc, argv, "--steam-root");
    if (!root.empty()) Steam::setRootOverride(root);
    if (flag(argc, argv, "--fake-running")) Steam::setFakeRunning(true);

    if (cmd == "detect") {
        auto s = Steam::status();
        printf("steam found=%d sandbox=%d running=%d accounts=%d\n", s.found, s.sandbox, s.running, (int)s.accounts.size());
        for (size_t i = 0; i < s.accounts.size(); i++)
            printf("  account #%zu%s\n", i + 1, s.accounts[i].mostRecent ? " (most recent)" : "");
        return s.found ? 0 : 1;
    }
    if (cmd == "validate-appids") {
        auto s = Steam::status();
        int entries = 0, match = 0, files = 0;
        for (auto& a : s.accounts) {
            std::vector<uint8_t> d;
            if (!vdf::readFile(Steam::shortcutsPath(s.root, a.accountId), d)) continue;
            files++;
            vdf::BNode r;
            if (!vdf::parseBinary(d, r)) { printf("  a shortcuts.vdf failed to parse\n"); continue; }
            std::string why;
            printf("  file %d: round-trip %s\n", files,
                   vdf::writeBinary(r) == d ? "byte-identical" : "DIFFERS");
            for (auto& e : r.find("shortcuts")->kids) {
                if (e.type != vdf::BType::Map) continue;
                entries++;
                auto ids = Steam::idsFor(e.getStr("Exe"), e.getStr("AppName"));
                if (e.find("appid") && ids.appid == e.getInt("appid")) match++;
            }
        }
        printf("validate-appids: %d shortcuts.vdf file(s), %d entries, %d appid matches\n", files, entries, match);
        return 0;
    }

    // ---- write commands: sandbox only
    if (cmd == "shortcut") {
        std::string dir = arg(argc, argv, "--dir");
        if (dir.empty()) { printf("refusing: --dir <sandbox> is required\n"); return 2; }
        Shortcuts::setDirOverride(dir);
        std::string sub = argc >= 3 ? argv[2] : "status", exe = arg(argc, argv, "--exe");
        for (auto p : {ShortcutPlace::Desktop, ShortcutPlace::StartMenu}) {
            if (sub == "create") {
                std::string err;
                bool ok = Shortcuts::create(p, exe, "", fs::u8path(exe).parent_path().u8string(), "", &err);
                printf("create %s: %s %s\n", Shortcuts::pathFor(p).c_str(), ok ? "ok" : "FAILED", err.c_str());
            } else if (sub == "remove") {
                printf("remove %s: %s\n", Shortcuts::pathFor(p).c_str(), Shortcuts::remove(p) ? "ok" : "not found");
            } else {
                printf("%s exists=%d\n", Shortcuts::pathFor(p).c_str(), Shortcuts::exists(p));
            }
        }
        return 0;
    }
    if (root.empty()) {
        printf("refusing: write commands need --steam-root <sandbox>\n");
        return 2;
    }
    auto accs = Steam::accounts(root);
    std::string acc = arg(argc, argv, "--account");
    if (acc.empty() && !accs.empty()) acc = accs.front().accountId;
    if (acc.empty()) { printf("no account in sandbox\n"); return 1; }
    ShortcutSpec spec;
    spec.exePath = arg(argc, argv, "--exe", "C:/Games/ZigguratRecomp/ZigguratLauncher.exe");
    spec.startDir = fs::u8path(spec.exePath).parent_path().u8string();
    if (cmd == "status") {
        auto st = Steam::entryStatus(root, acc, spec);
        printf("status: file=%d parsed=%d entries=%d added=%d index=%d gridId=%s appid=%d canUndo=%d %s\n", st.fileExists,
               st.parsed, st.entries, st.added, st.index, st.ids.gridId().c_str(), st.ids.appid, st.canUndo, st.error.c_str());
        return 0;
    }
    if (cmd == "add") {
        std::string art = arg(argc, argv, "--art");
        if (art.empty()) {
            std::string assets = arg(argc, argv, "--assets");
            art = root + "/_art_tmp";
            auto g = ArtworkGen::generate(assets, art);
            if (!g.ok) printf("artwork: %s\n", g.error.c_str());
        }
        auto r = Steam::addOrUpdate(root, acc, spec, art);
        printf("add: %s %s %s\n", r.ok ? "ok" : "FAILED", r.msgId.c_str(), r.detail.c_str());
        return r.ok ? 0 : 1;
    }
    if (cmd == "remove") {
        auto r = Steam::remove(root, acc, spec);
        printf("remove: %s %s %s\n", r.ok ? "ok" : "FAILED", r.msgId.c_str(), r.detail.c_str());
        return r.ok ? 0 : 1;
    }
    if (cmd == "undo") {
        auto r = Steam::undo(root, acc);
        printf("undo: %s %s %s\n", r.ok ? "ok" : "FAILED", r.msgId.c_str(), r.detail.c_str());
        return r.ok ? 0 : 1;
    }
    printf("unknown command\n");
    return 2;
}
