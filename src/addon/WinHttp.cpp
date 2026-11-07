#include "WinHttp.h"

#include <windows.h>
#include <winhttp.h>

#include <vector>

namespace wc {

    namespace {
        std::wstring Wide(const std::string& s) {
            if (s.empty()) return {};
            int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
            std::wstring w(static_cast<size_t>(n), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
            return w;
        }

        std::string Why(const char* step) { return std::string(step) + " failed (" + std::to_string(GetLastError()) + ")"; }

        struct Handle {
            HINTERNET h = nullptr;
            ~Handle() {
                if (h) WinHttpCloseHandle(h);
            }
        };
    }

    WinHttp::WinHttp() {
        HINTERNET session = WinHttpOpen(L"WondrousCompass-Nexus/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session) {
            // older Windows has no automatic proxy: the default one then
            session = WinHttpOpen(L"WondrousCompass-Nexus/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        }
        if (session) {
            // connect, send and wait for the answer: twenty seconds at most each, as the module waited
            WinHttpSetTimeouts(session, 10000, 10000, 20000, 20000);
        }
        m_session = session;
    }

    WinHttp::~WinHttp() { Abort(); }

    void WinHttp::Abort() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_request) {
            WinHttpCloseHandle(static_cast<HINTERNET>(m_request));
            m_request = nullptr;
        }
        if (m_session) {
            WinHttpCloseHandle(static_cast<HINTERNET>(m_session));
            m_session = nullptr;
        }
    }

    HttpResponse WinHttp::Post(const std::string& url, const std::map<std::string, std::string>& headers, const std::string& body) {
        HttpResponse res;
        HINTERNET session;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            session = static_cast<HINTERNET>(m_session);
        }
        if (!session) {
            res.error = "no session";
            return res;
        }

        std::wstring wurl = Wide(url);
        URL_COMPONENTS parts{};
        parts.dwStructSize = sizeof(parts);
        wchar_t host[256] = {}, path[2048] = {};
        parts.lpszHostName = host;
        parts.dwHostNameLength = 256;
        parts.lpszUrlPath = path;
        parts.dwUrlPathLength = 2048;
        if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &parts)) {
            res.error = Why("WinHttpCrackUrl");
            return res;
        }

        Handle connect{WinHttpConnect(session, host, parts.nPort, 0)};
        if (!connect.h) {
            res.error = Why("WinHttpConnect");
            return res;
        }
        DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET opened = WinHttpOpenRequest(connect.h, L"POST", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (!opened) {
            res.error = Why("WinHttpOpenRequest");
            return res;
        }
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_session) {
                WinHttpCloseHandle(opened);
                res.error = "aborted";
                return res;
            }
            m_request = opened;
        }
        // the request is closed here when it ends, or by Abort when the addon unloads meanwhile
        struct Owned {
            WinHttp* self;
            HINTERNET h;
            ~Owned() {
                std::lock_guard<std::mutex> lock(self->m_mutex);
                if (self->m_request == h) {
                    WinHttpCloseHandle(h);
                    self->m_request = nullptr;
                }
            }
        } request{this, opened};

        std::wstring head;
        for (const auto& kv : headers) head += Wide(kv.first) + L": " + Wide(kv.second) + L"\r\n";
        if (!WinHttpSendRequest(request.h, head.c_str(), static_cast<DWORD>(-1L), const_cast<char*>(body.data()), static_cast<DWORD>(body.size()),
                                static_cast<DWORD>(body.size()), 0)) {
            res.error = Why("WinHttpSendRequest");
            return res;
        }
        if (!WinHttpReceiveResponse(request.h, nullptr)) {
            res.error = Why("WinHttpReceiveResponse");
            return res;
        }

        DWORD status = 0, size = sizeof(status);
        if (!WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size,
                                 WINHTTP_NO_HEADER_INDEX)) {
            res.error = Why("WinHttpQueryHeaders");
            return res;
        }

        std::string text;
        for (;;) {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(request.h, &available)) {
                res.error = Why("WinHttpQueryDataAvailable");
                return res;
            }
            if (available == 0) break;
            std::vector<char> buffer(available);
            DWORD read = 0;
            if (!WinHttpReadData(request.h, buffer.data(), available, &read)) {
                res.error = Why("WinHttpReadData");
                return res;
            }
            text.append(buffer.data(), read);
            // an answer of the server is small; anything past a few megabytes is not one
            if (text.size() > 8 * 1024 * 1024) {
                res.error = "answer too large";
                return res;
            }
        }
        res.status = static_cast<int>(status);
        res.body = std::move(text);
        return res;
    }
}
