// A minimal HTTP/1.1 client for a model server on this machine (llama-server). Kept out of the
// dialogue library so engine builds can use their own HTTP stack instead (sovereign_diplomacy_http).
#pragma once

#include <string>

#include "sovereign_diplomacy/dialogue.h"

namespace sov::diplomacy {

class SocketTransport : public Transport {
public:
    // Local hosts only ("127.0.0.1", "localhost", "::1"): player chat never leaves the machine.
    SocketTransport(std::string host = "127.0.0.1", int port = 8080, int timeoutSeconds = 30);
    bool postJson(const std::string& path, const std::string& body, std::string& response) override;
    // GET /health answers 200 once the model is loaded.
    bool healthy();
    bool local() const { return local_; }

private:
    bool request(const std::string& head, const std::string& body, std::string& response);
    std::string host_;
    int port_;
    int timeout_;
    bool local_;
};

// Pulls the body out of a raw HTTP response (Content-Length or chunked); false unless 2xx.
bool parseHttpResponse(const std::string& raw, std::string& body);

}  // namespace sov::diplomacy
