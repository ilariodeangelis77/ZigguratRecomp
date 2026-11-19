// zigextras: tests for the optional extras (Steam Input mode, update check, reference mode).
//   zigextras selftest
//   zigextras si-get   <localconfig.vdf> <shortId>
//   zigextras si-set   --steam-root <sandbox> <accountId> <shortId> default|enabled|disabled
//   zigextras update   <feedUrl> [currentVersion]
//   zigextras ref-app  <dumpSource>            (extracts into <data>/reference when given an .ipa)
//   zigextras ref-find [configuredPath]
#include <SDL3/SDL_main.h>
#include "core/FileIO.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

#include "core/Reference.h"
#include "core/Steam.h"
#include "core/SteamInput.h"
#include "core/UpdateCheck.h"
#include "core/Version.h"

using namespace zig;

static int fails = 0;
static void expect(bool c, const char* what) {
    if (!c) { fails++; printf("FAIL: %s\n", what); }
}

static int selftest() {
    // ---- Steam Input text splicing
    const std::string base =
        "\"UserLocalConfigStore\"\n{\n\t\"Software\"\n\t{\n\t\t\"x\"\t\t\"1\"\n\t}\n\t\"apps\"\n\t{\n\t\t\"123\"\n\t\t{\n"
        "\t\t\t\"LastPlayed\"\t\t\"5\"\n\t\t}\n\t}\n}\n";
    std::string err;
    // insert into an existing app block? (our id absent) -> new block in apps
    std::string a = SteamInput::apply(base, "2925103281", "enabled", err);
    expect(!a.empty(), "apply enabled (new block)");
    expect(SteamInput::get(a, "2925103281") == "enabled", "get enabled");
    expect(SteamInput::get(a, "123") == "default", "other app untouched");
    expect(a.find("\"LastPlayed\"\t\t\"5\"") != std::string::npos, "other keys preserved");
    // replace value
    std::string b = SteamInput::apply(a, "2925103281", "disabled", err);
    expect(SteamInput::get(b, "2925103281") == "disabled", "replace -> disabled");
    expect(b.size() == a.size(), "value replaced in place");
    // default removes the key line only
    std::string c = SteamInput::apply(b, "2925103281", "default", err);
    expect(SteamInput::get(c, "2925103281") == "default", "default removes key");
    expect(c == base, "set -> default round trip is byte-identical (created block removed)");
    std::string d2 = SteamInput::apply(SteamInput::apply(base, "123", "enabled", err), "123", "default", err);
    expect(d2 == base, "existing block: only our key removed");
    // insert key into existing app block
    std::string d = SteamInput::apply(base, "123", "enabled", err);
    expect(SteamInput::get(d, "123") == "enabled", "insert into existing block");
    expect(d.find("\"LastPlayed\"") != std::string::npos, "existing key kept");
    // default on a file without the key = identical bytes
    expect(SteamInput::apply(base, "999", "default", err) == base, "default no-op identical");
    // file without apps
    std::string noapps = "\"UserLocalConfigStore\"\r\n{\r\n\t\"Software\"\r\n\t{\r\n\t}\r\n}\r\n";
    std::string e = SteamInput::apply(noapps, "42", "disabled", err);
    expect(SteamInput::get(e, "42") == "disabled", "create apps block");
    expect(SteamInput::apply(e, "42", "default", err) == noapps, "created apps block removed again (identical)");
    expect(e.find("\r\n") != std::string::npos && e.find("\n\t\"apps\"") != std::string::npos, "CRLF kept");
    // escaped quotes and comments survive
    std::string q = "// comment\n\"UserLocalConfigStore\"\n{\n\t\"k\"\t\t\"a \\\"q\\\" b\"\n\t\"apps\"\n\t{\n\t}\n}\n";
    std::string f = SteamInput::apply(q, "7", "enabled", err);
    expect(f.find("// comment") == 0 && f.find("a \\\"q\\\" b") != std::string::npos, "comments/escapes kept");
    expect(SteamInput::get(f, "7") == "enabled", "get after escapes");
    // broken file
    expect(SteamInput::apply("\"UserLocalConfigStore\"\n{\n", "1", "enabled", err).empty(), "broken file rejected");

    // ---- versions + feed
    expect(UpdateCheck::compareVersions("0.9.0", "0.9.0") == 0, "ver equal");
    expect(UpdateCheck::compareVersions("0.10.0", "0.9.9") > 0, "ver numeric");
    expect(UpdateCheck::compareVersions("v1.0", "0.9.0") > 0, "ver v-prefix");
    expect(UpdateCheck::compareVersions("0.9", "0.9.1") < 0, "ver shorter");
    UpdateInfo u;
    expect(UpdateCheck::parseFeed("{\"version\":\"1.0.0\",\"url\":\"https://x/y\",\"notes\":\"hi\"}", u) && u.url == "https://x/y",
           "feed parse");
    UpdateInfo u2;
    expect(UpdateCheck::parseFeed("{\"version\":\"1.0.0\",\"url\":\"file:///c:/evil.exe\"}", u2) && u2.url.empty(),
           "non-http url dropped");
    UpdateInfo u3;
    expect(!UpdateCheck::parseFeed("not json", u3), "bad feed rejected");
    expect(UpdateCheck::check("", kVersion).msgId == "extras.upd.nofeed", "empty feed -> nofeed");

    printf("zigextras selftest: %s (%d failures)\n", fails ? "FAILED" : "passed", fails);
    return fails ? 1 : 0;
}

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: see source header\n"); return 2; }
    std::string cmd = argv[1];
    if (cmd == "selftest") return selftest();
    if (cmd == "si-get" && argc >= 4) {
        std::ifstream in(zig::u8p(argv[2]), std::ios::binary);
        std::string t((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        printf("%s\n", SteamInput::get(t, argv[3]).c_str());
        return 0;
    }
    if (cmd == "si-set" && argc >= 7 && !strcmp(argv[2], "--steam-root")) {
        Steam::setRootOverride(argv[3]);
        if (!Steam::isSandbox()) { printf("refusing: not a sandbox root\n"); return 3; }
        auto r = SteamInput::write(argv[3], argv[4], argv[5], argv[6]);
        printf("%s %s %s\n", r.ok ? "ok" : "error", r.msgId.c_str(), r.detail.c_str());
        return r.ok ? 0 : 1;
    }
    if (cmd == "update" && argc >= 3) {
        auto r = UpdateCheck::check(argv[2], argc >= 4 ? argv[3] : kVersion);
        printf("ok=%d newer=%d msg=%s version=%s url=%s notes=%s detail=%s\n", r.ok, r.newer, r.msgId.c_str(),
               r.version.c_str(), r.url.c_str(), r.notes.c_str(), r.detail.c_str());
        return r.ok ? 0 : 1;
    }
    if (cmd == "ref-app" && argc >= 3) {
        std::string err, app = Reference::appFolder(argv[2], err);
        printf("%s\n", app.empty() ? ("error: " + err).c_str() : app.c_str());
        return app.empty() ? 1 : 0;
    }
    if (cmd == "ref-find") {
        printf("%s\n", Reference::findTouchHLE(argc >= 3 ? argv[2] : "").c_str());
        return 0;
    }
    printf("unknown command\n");
    return 2;
}
