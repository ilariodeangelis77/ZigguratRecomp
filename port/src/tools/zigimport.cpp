// zigimport: command-line front-end for zigcore dump import (tests + manual use).
//   zigimport scan    <path>
//   zigimport inspect <path>
//   zigimport verify  <path>
//   zigimport import  <path> <destAssetsDir> [--allow-unverified]
//   zigimport compare <dirA> <dirB>        (PNG: decoded pixels; JSON: parsed; others: bytes)
#include <SDL3/SDL_main.h>
#include "core/FileIO.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "core/DumpImport.h"
#include "json.hpp"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "stb_image.h"

namespace fs = std::filesystem;
using namespace zig;

static bool readAll(const fs::path& p, std::vector<uint8_t>& out) {
    std::ifstream in(p, std::ios::binary);  // fs::path: wide on Windows
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), {});
    return true;
}

static int usage() {
    fprintf(stderr, "usage: zigimport scan|inspect|verify <path> | import <path> <dest> [--allow-unverified] | compare <dirA> <dirB>\n");
    return 64;
}

static bool pickCandidate(const std::string& path, DumpCandidate& c) {
    ScanResult s = scanForDumps(path);
    if (s.candidates.empty()) {
        printf("status: %s\nmessage[%s]: %s\n", toString(s.status), s.message.id.c_str(), s.message.text.c_str());
        return false;
    }
    c = s.candidates[0];
    return true;
}

static int cmdCompare(const std::string& a, const std::string& b) {
    int files = 0, diffs = 0, missing = 0;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(fs::u8path(a), ec); it != fs::recursive_directory_iterator(); ++it) {
        if (!it->is_regular_file()) continue;
        fs::path rel = fs::relative(it->path(), fs::u8path(a));
        fs::path other = fs::u8path(b) / rel;
        files++;
        std::vector<uint8_t> da, db;
        if (!readAll(other, db)) { printf("MISSING in B: %s\n", rel.u8string().c_str()); missing++; continue; }
        readAll(it->path(), da);
        std::string x = it->path().extension().u8string();
        bool same;
        if (x == ".png") {
            int wa, ha, na, wb, hb, nb;
            unsigned char* pa = stbi_load_from_memory(da.data(), (int)da.size(), &wa, &ha, &na, 4);
            unsigned char* pb = stbi_load_from_memory(db.data(), (int)db.size(), &wb, &hb, &nb, 4);
            same = pa && pb && wa == wb && ha == hb && memcmp(pa, pb, (size_t)wa * ha * 4) == 0;
            stbi_image_free(pa);
            stbi_image_free(pb);
        } else if (x == ".json") {
            auto ja = nlohmann::json::parse(da.begin(), da.end(), nullptr, false);
            auto jb = nlohmann::json::parse(db.begin(), db.end(), nullptr, false);
            same = !ja.is_discarded() && ja == jb;
        } else {
            same = da == db;
        }
        if (!same) { printf("DIFF: %s\n", rel.u8string().c_str()); diffs++; }
    }
    // files only in B
    int extra = 0;
    for (auto it = fs::recursive_directory_iterator(fs::u8path(b), ec); it != fs::recursive_directory_iterator(); ++it)
        if (it->is_regular_file() && !fs::exists(fs::u8path(a) / fs::relative(it->path(), fs::u8path(b)))) {
            printf("ONLY in B: %s\n", fs::relative(it->path(), fs::u8path(b)).u8string().c_str());
            extra++;
        }
    printf("compared %d files: %d differ, %d missing in B, %d only in B\n", files, diffs, missing, extra);
    return (diffs || missing) ? 1 : 0;
}

int main(int argc, char** argv) {
    if (argc < 3) return usage();
    std::string cmd = argv[1], path = argv[2];
    if (cmd == "scan") {
        ScanResult s = scanForDumps(path);
        printf("candidates: %zu\n", s.candidates.size());
        for (auto& c : s.candidates)
            printf("  [%s] %s  (%s)\n", c.kind == DumpCandidate::Kind::Ipa ? "ipa" : "app", c.label.c_str(), c.path.c_str());
        if (s.candidates.empty())
            printf("status: %s\nmessage[%s]: %s\n", toString(s.status), s.message.id.c_str(), s.message.text.c_str());
        return s.candidates.empty() ? 1 : 0;
    }
    if (cmd == "compare") {
        if (argc < 4) return usage();
        return cmdCompare(path, argv[3]);
    }
    DumpCandidate c;
    if (!pickCandidate(path, c)) return 1;
    if (cmd == "inspect") {
        DumpInfo i = inspectDump(c);
        if (!i.ok) { printf("error[%s]: %s\n", i.error.id.c_str(), i.error.text.c_str()); return 1; }
        printf("bundle: %s\nversion: %s\nname: %s\nfiles: %zu\n", i.bundleId.c_str(), i.version.c_str(),
               i.displayName.c_str(), i.files.size());
        return 0;
    }
    if (cmd == "verify") {
        auto t0 = std::chrono::steady_clock::now();
        VerifyResult v = verifyDump(c);
        double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        printf("candidate: %s\nstatus: %s\nversion: %s\nok/missing/modified/total: %d/%d/%d/%d\nmessage[%s]: %s\ntime: %.2fs\n",
               c.path.c_str(), toString(v.status), v.version.c_str(), v.ok, v.missing, v.modified, v.total,
               v.message.id.c_str(), v.message.text.c_str(), s);
        return v.status == DumpStatus::Verified ? 0 : 1;
    }
    if (cmd == "import") {
        if (argc < 4) return usage();
        bool allow = argc > 4 && !strcmp(argv[4], "--allow-unverified");
        int lastPct = -1;
        ImportResult r = importDump(c, argv[3], [&](int done, int total, const std::string&) {
            int pct = total ? done * 100 / total : 100;
            if (pct / 25 != lastPct / 25) { printf("  %d%%\n", pct); lastPct = pct; }
        }, nullptr, allow);
        printf("ok: %d\nmessage[%s]: %s\npngs %d audio %d fonts %d strings %d other %d bytes %llu time %.2fs\n", (int)r.ok,
               r.message.id.c_str(), r.message.text.c_str(), r.pngs, r.audio, r.fonts, r.strings, r.other,
               (unsigned long long)r.bytesWritten, r.seconds);
        return r.ok ? 0 : 1;
    }
    return usage();
}
