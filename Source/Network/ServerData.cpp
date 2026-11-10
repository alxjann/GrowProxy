#include "Network/ServerData.h"

#include "Core/Config.h"
#include "Core/Log.h"
#include "Network/Http.h"
#include "Utils/Strings.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cctype>
#include <map>
#include <optional>

namespace {

    // DNS lookups for the game domain.
    namespace Dns {

        std::optional<uint32> ParseIPv4(string_view text) {
            if (text.empty() || text.size() > 15)
                return std::nullopt;

            string copy(text);
            in_addr addr{};
            if (inet_pton(AF_INET, copy.c_str(), &addr) != 1)
                return std::nullopt;
            return ntohl(addr.s_addr);
        }

        // https://www.iana.org/assignments/iana-ipv4-special-registry
        bool IsLocalAddress(string_view ip) {
            std::optional<uint32> addr = ParseIPv4(ip);
            if (!addr)
                return true;

            uint32 a = *addr;
            return a == 0 || (a & 0xFF000000) == 0x7F000000 || // 127.0.0.0/8
                   (a & 0xFF000000) == 0x0A000000 ||           // 10.0.0.0/8
                   (a & 0xFFF00000) == 0xAC100000 ||           // 172.16.0.0/12
                   (a & 0xFFFF0000) == 0xC0A80000;             // 192.168.0.0/16
        }

        string Resolve(const string& domain) {
            addrinfo hints{};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;

            addrinfo* result = nullptr;
            if (getaddrinfo(domain.c_str(), nullptr, &hints, &result) != 0 || !result)
                return "";

            char ip[INET_ADDRSTRLEN] = {};
            inet_ntop(AF_INET, &((sockaddr_in*)result->ai_addr)->sin_addr, ip, sizeof(ip));
            freeaddrinfo(result);
            return ip;
        }

        string ResolveOverHttps(const string& provider, const string& domain) {
            string name = domain;

            for (int depth = 0; depth < 5; ++depth) {
                HttpClient http(provider);
                http.SetHeader("Accept", "application/dns-json");
                if (!http.Get("/dns-query?name=" + name + "&type=A") || http.GetStatus() != 200)
                    return "";

                // answers look like "data":"1.2.3.4" (A record) or "data":"other.domain." (CNAME)
                const string& json = http.GetBody();
                string cname;
                size_t pos = 0;
                while ((pos = json.find("\"data\":\"", pos)) != string::npos) {
                    pos += 8;
                    size_t end = json.find('"', pos);
                    if (end == string::npos)
                        break;

                    string value = json.substr(pos, end - pos);
                    if (!value.empty() && value.back() == '.')
                        value.pop_back();

                    if (ParseIPv4(value)) {
                        if (!IsLocalAddress(value))
                            return value;
                    } else if (cname.empty() && value.find('.') != string::npos) {
                        cname = value;
                    }
                    pos = end;
                }

                if (cname.empty())
                    return "";
                name = cname;
            }
            return "";
        }

    }

    constexpr const char* SERVER_DATA_PATH = "/growtopia/server_data.php";

    string DecodeFormValue(string_view text) {
        string out;
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '+') {
                out += ' ';
            } else if (text[i] == '%' && i + 2 < text.size() && std::isxdigit((unsigned char)text[i + 1]) &&
                       std::isxdigit((unsigned char)text[i + 2])) {
                out += (char)std::stoi(string(text.substr(i + 1, 2)), nullptr, 16);
                i += 2;
            } else {
                out += text[i];
            }
        }
        return out;
    }

    // "a=1&b=2" -> {a: 1, b: 2}. Empty values are dropped and the first value of a key wins.
    std::map<string, string> ParseForm(string_view body) {
        std::map<string, string> fields;
        while (!body.empty()) {
            size_t amp = body.find('&');
            string_view pair = body.substr(0, amp);
            body = amp == string_view::npos ? string_view() : body.substr(amp + 1);

            size_t eq = pair.find('=');
            if (eq == string_view::npos)
                continue;

            string value = DecodeFormValue(pair.substr(eq + 1));
            if (!value.empty())
                fields.emplace(DecodeFormValue(pair.substr(0, eq)), value);
        }
        return fields;
    }

    // The real game server address. The OS resolver is tried first; if it answers with a local address (the
    // hosts file points the game domains at this machine) DNS-over-HTTPS is used instead.
    string FindGameServerIP(string& outDomain) {
        for (const char* domain : Config::GAME_DOMAINS) {
            string ip = Dns::Resolve(domain);
            if (!ip.empty() && !Dns::IsLocalAddress(ip)) {
                outDomain = domain;
                return ip;
            }
        }

        for (const char* domain : Config::GAME_DOMAINS) {
            for (const char* provider : Config::DOH_PROVIDERS) {
                string ip = Dns::ResolveOverHttps(provider, domain);
                if (!ip.empty()) {
                    outDomain = domain;
                    return ip;
                }
            }
        }
        return "";
    }

}

bool RedirectServerData(const string& data, const Endpoint& target, string& outData, Endpoint& outOriginal) {
    outData.clear();
    outOriginal = {};

    string_view rest = data;
    bool first = true;
    while (!rest.empty()) {
        size_t newline = rest.find('\n');
        string line(rest.substr(0, newline));
        rest = newline == string_view::npos ? string_view() : rest.substr(newline + 1);

        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        size_t bar = line.find('|');
        if (bar != string::npos && line[0] != '#') {
            string_view key = Strings::Trim(string_view(line).substr(0, bar));
            string_view value = Strings::Trim(string_view(line).substr(bar + 1));

            if (key == "server") {
                outOriginal.host = string(value);
                line = "server|" + target.host;
            } else if (key == "port") {
                int32 port = Strings::ToInt(value).value_or(0);
                outOriginal.port = port > 0 && port <= 65535 ? (uint16)port : 0;
                line = "port|" + std::to_string(target.port);
            }
        }

        if (!first)
            outData += '\n';
        outData += line;
        first = false;
    }

    return !outOriginal.host.empty() && outOriginal.port != 0;
}

HttpResponse ServerData::Handle(const HttpRequest& request) {
    if (request.path != SERVER_DATA_PATH)
        return {404, "Not Found"};

    Log::Info("[HTTP] %s server_data.php: %s", request.method.c_str(), request.body.c_str());

    auto form = ParseForm(request.body);
    if (!form.count("protocol") || !form.count("version") || !form.count("platform"))
        return {400, "Error: Bad Params"};

    if (!Strings::ToInt(form["protocol"]) || !Strings::ToFloat(form["version"]) || !Strings::ToInt(form["platform"]))
        return {400, "Error: Invalid Data Types"};

    string serverData =
        Fetch(form["protocol"], form["version"], form["platform"], string(request.GetHeader("user-agent")));

    string redirected;
    Endpoint gameServer;
    if (serverData.empty() ||
        !RedirectServerData(serverData, {Config::LOCAL_HOST, Config::LOCAL_ENET_PORT}, redirected, gameServer)) {
        if (!serverData.empty())
            Log::Error("[HTTP] Server data has no valid server/port");
        return {500, ""};
    }

    Log::Info("[HTTP] Game server is %s:%u, redirecting the client to the proxy", gameServer.host.c_str(),
              gameServer.port);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_gameServer = gameServer;
    }
    return {200, redirected};
}

string ServerData::Fetch(const string& protocol, const string& version, const string& platform,
                         const string& userAgent) {
    string domain;
    string ip = FindGameServerIP(domain);
    if (ip.empty()) {
        Log::Error("[HTTP] Could not resolve the game server");
        return "";
    }

    HttpClient http(ip);
    http.AddFormField("version", version);
    http.AddFormField("platform", platform);
    http.AddFormField("protocol", protocol);
    http.SetHeader("User-Agent", userAgent);
    http.SetHeader("Host", domain);

    if (!http.Post(SERVER_DATA_PATH) || http.GetBody().empty()) {
        Log::Error("[HTTP] server_data.php request to %s failed (status %u)", ip.c_str(), http.GetStatus());
        return "";
    }
    return http.GetBody();
}

std::optional<Endpoint> ServerData::GetGameServer() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_gameServer;
}
