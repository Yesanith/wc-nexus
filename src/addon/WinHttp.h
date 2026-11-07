#pragma once

#include <mutex>

#include "core/Api.h"

namespace wc {

    /// the requests of the addon, through WinHTTP: the HTTP of Windows, which knows the certificates and the proxy of the machine
    class WinHttp : public IHttp {
    public:
        WinHttp();
        ~WinHttp() override;

        HttpResponse Post(const std::string& url, const std::map<std::string, std::string>& headers, const std::string& body) override;

        /// closes the session: a request under way ends at once, and no other is made
        void Abort();

    private:
        std::mutex m_mutex;
        void* m_session = nullptr;  // HINTERNET
        void* m_request = nullptr;  // the request under way, which Abort closes to end it
    };
}
