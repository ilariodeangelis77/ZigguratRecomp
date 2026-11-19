#include "core/UpdateCheck.h"

#include <SDL3/SDL.h>

#include <cstdlib>
#include <vector>

#include "json.hpp"

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

namespace zig {

int UpdateCheck::compareVersions(const std::string& a, const std::string& b) {
    auto parts = [](const std::string& s) {
        std::vector<long> v;
        size_t i = 0;
        if (!s.empty() && (s[0] == 'v' || s[0] == 'V')) i = 1;
        while (i < s.size()) {
            size_t j = i;
            while (j < s.size() && s[j] != '.') j++;
            v.push_back(std::strtol(s.substr(i, j - i).c_str(), nullptr, 10));
            i = j + 1;
        }
        return v;
    };
    auto pa = parts(a), pb = parts(b);
    size_t n = std::max(pa.size(), pb.size());
    for (size_t i = 0; i < n; i++) {
        long x = i < pa.size() ? pa[i] : 0, y = i < pb.size() ? pb[i] : 0;
        if (x != y) return x < y ? -1 : 1;
    }
    return 0;
}

bool UpdateCheck::parseFeed(const std::string& body, UpdateInfo& out) {
    auto j = nlohmann::json::parse(body, nullptr, false);
    if (j.is_discarded() || !j.is_object() || !j.contains("version") || !j["version"].is_string()) return false;
    out.version = j["version"].get<std::string>();
    if (j.contains("url") && j["url"].is_string()) out.url = j["url"].get<std::string>();
    if (j.contains("notes") && j["notes"].is_string()) out.notes = j["notes"].get<std::string>();
    // Only http(s) download pages are offered to SDL_OpenURL.
    if (!(out.url.rfind("https://", 0) == 0 || out.url.rfind("http://", 0) == 0)) out.url.clear();
    if (out.notes.size() > 600) out.notes = out.notes.substr(0, 600) + "...";
    return true;
}

bool UpdateCheck::httpGet(const std::string& url, std::string& body, std::string& err) {
#ifdef _WIN32
    int wl = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, nullptr, 0);
    std::wstring wurl(wl > 0 ? wl - 1 : 0, L'\0');
    if (wl > 0) MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, &wurl[0], wl);
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof uc;
    wchar_t host[256] = {}, path[2048] = {};
    uc.lpszHostName = host;
    uc.dwHostNameLength = 255;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 2047;
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) { err = "bad url"; return false; }
    HINTERNET ses = WinHttpOpen(L"ZigguratLauncher-UpdateCheck", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) { err = "WinHttpOpen failed"; return false; }
    WinHttpSetTimeouts(ses, 5000, 5000, 5000, 8000);
    HINTERNET con = WinHttpConnect(ses, host, uc.nPort, 0);
    HINTERNET req = con ? WinHttpOpenRequest(con, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
                        : nullptr;
    bool ok = req && WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
              WinHttpReceiveResponse(req, nullptr);
    DWORD status = 0, sz = sizeof status;
    if (ok) WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                                &status, &sz, WINHTTP_NO_HEADER_INDEX);
    if (ok && status == 200) {
        for (;;) {
            DWORD avail = 0, got = 0;
            if (!WinHttpQueryDataAvailable(req, &avail) || avail == 0) break;
            std::string chunk(avail, '\0');
            if (!WinHttpReadData(req, &chunk[0], avail, &got)) break;
            body.append(chunk.data(), got);
            if (body.size() > 64 * 1024) break;   // a feed is tiny; never read more
        }
    } else {
        err = ok ? ("HTTP " + std::to_string(status)) : "connection failed";
        ok = false;
    }
    if (req) WinHttpCloseHandle(req);
    if (con) WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return ok;
#else
    (void)url; (void)body;
    err = "update check is only implemented on Windows";
    return false;
#endif
}

UpdateInfo UpdateCheck::check(const std::string& feedUrl, const std::string& current) {
    UpdateInfo r;
    if (feedUrl.empty()) { r.msgId = "extras.upd.nofeed"; return r; }
    std::string body, err;
    if (!httpGet(feedUrl, body, err)) {
        r.msgId = "extras.upd.err.net";
        r.detail = err;
        SDL_Log("update check: %s", err.c_str());
        return r;
    }
    if (!parseFeed(body, r)) { r.msgId = "extras.upd.err.feed"; return r; }
    r.ok = true;
    r.newer = compareVersions(r.version, current) > 0;
    r.msgId = r.newer ? "extras.upd.newer" : "extras.upd.latest";
    SDL_Log("update check: feed version %s, current %s", r.version.c_str(), current.c_str());
    return r;
}

}  // namespace zig
