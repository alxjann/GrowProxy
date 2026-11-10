#pragma once

#include "Network/Http.h"

#include <mutex>
#include <optional>

struct Endpoint {
    string host;
    uint16 port = 0;
};

// Answers the client's POST /growtopia/server_data.php: fetches the real server data, remembers the real game
// server and replaces it with the local proxy so the client connects to us. Called from HTTP server threads.
class ServerData {
public:
    HttpResponse Handle(const HttpRequest& request);

    // Real game server from the last successful request.
    std::optional<Endpoint> GetGameServer() const;

private:
    string Fetch(const string& protocol, const string& version, const string& platform, const string& userAgent);

    mutable std::mutex m_mutex;
    std::optional<Endpoint> m_gameServer;
};

// Replaces the server|/port| lines and reports the original values. Exposed for tests.
bool RedirectServerData(const string& data, const Endpoint& target, string& outData, Endpoint& outOriginal);
