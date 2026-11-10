#pragma once

#include "Core/Types.h"

namespace Config {
    // The game client is redirected to this local ENet server.
    constexpr const char* LOCAL_HOST = "127.0.0.1";
    constexpr uint16 LOCAL_ENET_PORT = 19000;

    // Ports the client uses for server_data.php (the hosts file points Growtopia's domains at this machine).
    constexpr uint16 HTTP_PORT = 80;
    constexpr uint16 HTTPS_PORT = 443;

    // Domains used to find the real game server.
    constexpr const char* GAME_DOMAINS[] = {"www.growtopia2.com", "growtopia2.com"};
    constexpr const char* DOH_PROVIDERS[] = {"https://1.1.1.1", "https://8.8.8.8"};

    constexpr uint32 SERVER_CONNECT_TIMEOUT_MS = 5000;
}
