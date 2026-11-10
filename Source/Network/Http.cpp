#include "Network/Http.h"

#include "Core/Log.h"
#include "Core/Platform.h"
#include "Utils/Strings.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <windows.h>
#include <winhttp.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

// HTTP client

namespace {

    struct HandleCloser {
        void operator()(HINTERNET handle) const { WinHttpCloseHandle(handle); }
    };
    using Handle = std::unique_ptr<void, HandleCloser>;

    string FormEncode(string_view text) {
        static const char* hex = "0123456789ABCDEF";
        string out;
        for (unsigned char c : text) {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                out += (char)c;
            } else {
                out += '%';
                out += hex[c >> 4];
                out += hex[c & 15];
            }
        }
        return out;
    }

}

HttpClient::HttpClient(string_view server) {
    if (Strings::StartsWith(server, "http://")) {
        server.remove_prefix(7);
        m_secure = false;
        m_port = 80;
    } else if (Strings::StartsWith(server, "https://")) {
        server.remove_prefix(8);
    }

    server = server.substr(0, server.find('/'));

    size_t colon = server.find(':');
    if (colon != string_view::npos) {
        m_port = (uint16)Strings::ToInt(server.substr(colon + 1)).value_or(m_port);
        server = server.substr(0, colon);
    }
    m_host = string(server);
}

void HttpClient::AddFormField(string_view name, string_view value) {
    if (!m_form.empty())
        m_form += '&';
    m_form += FormEncode(name) + "=" + FormEncode(value);
    m_headers.emplace("Content-Type", "application/x-www-form-urlencoded");
}

bool HttpClient::Get(const string& path) {
    return Send(L"GET", path);
}

bool HttpClient::Post(const string& path) {
    return Send(L"POST", path);
}

bool HttpClient::DownloadTo(const string& path, const string& filePath) {
    return Send(L"GET", path, filePath);
}

bool HttpClient::Send(const wchar_t* method, const string& path, const string& filePath) {
    m_status = 0;
    m_body.clear();

    Handle session(WinHttpOpen(L"Proxy/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                               WINHTTP_NO_PROXY_BYPASS, 0));
    Handle connection(session ? WinHttpConnect(session.get(), Platform::ToWide(m_host).c_str(), m_port, 0) : nullptr);
    Handle request(connection ? WinHttpOpenRequest(connection.get(), method, Platform::ToWide(path).c_str(), nullptr,
                                                   WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                   m_secure ? WINHTTP_FLAG_SECURE : 0)
                              : nullptr);
    if (!request) {
        Log::Error("[HTTP] Could not open %s%s (error %lu)", m_host.c_str(), path.c_str(), GetLastError());
        return false;
    }

    if (m_secure) {
        DWORD ignore = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                       SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
        WinHttpSetOption(request.get(), WINHTTP_OPTION_SECURITY_FLAGS, &ignore, sizeof(ignore));
    }

    for (const auto& [name, value] : m_headers) {
        std::wstring header = Platform::ToWide(name + ": " + value);
        WinHttpAddRequestHeaders(request.get(), header.c_str(), (DWORD)-1,
                                 WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    }

    void* body = m_form.empty() ? WINHTTP_NO_REQUEST_DATA : m_form.data();
    DWORD bodySize = (DWORD)m_form.size();
    if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, body, bodySize, bodySize, 0) ||
        !WinHttpReceiveResponse(request.get(), nullptr)) {
        Log::Error("[HTTP] Request to %s%s failed (error %lu)", m_host.c_str(), path.c_str(), GetLastError());
        return false;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
    m_status = (uint16)status;

    FILE* file = nullptr;
    if (!filePath.empty() && _wfopen_s(&file, Platform::ToWide(filePath).c_str(), L"wb") != 0) {
        Log::Error("[HTTP] Could not write %s", filePath.c_str());
        return false;
    }

    char buffer[8192];
    DWORD read = 0;
    while (WinHttpReadData(request.get(), buffer, sizeof(buffer), &read) && read > 0) {
        if (file)
            std::fwrite(buffer, 1, read, file);
        else
            m_body.append(buffer, read);
    }

    if (file)
        std::fclose(file);

    return m_status >= 200 && m_status < 300;
}

// HTTP server

namespace {

    constexpr DWORD IO_TIMEOUT_MS = 5000;
    constexpr size_t MAX_HEADER_SIZE = 16 * 1024;
    constexpr size_t MAX_BODY_SIZE = 64 * 1024;

    string TlsError(int code) {
        char text[128];
        mbedtls_strerror(code, text, sizeof(text));
        return text;
    }

    bool ReadFile(const string& path, std::vector<unsigned char>& out) {
        std::ifstream file(Platform::ToWide(path), std::ios::binary);
        if (!file)
            return false;
        out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        return true;
    }

    int SocketSend(void* socket, const unsigned char* data, size_t size) {
        int sent = send(*(SOCKET*)socket, (const char*)data, (int)size, 0);
        return sent < 0 ? MBEDTLS_ERR_NET_SEND_FAILED : sent;
    }

    int SocketRecv(void* socket, unsigned char* data, size_t size) {
        int received = recv(*(SOCKET*)socket, (char*)data, (int)size, 0);
        return received < 0 ? MBEDTLS_ERR_NET_RECV_FAILED : received;
    }

    // One accepted connection, plain or TLS. Owns the socket.
    class Connection {
    public:
        Connection(SOCKET socket, mbedtls_ssl_config* tls) : m_socket(socket) {
            DWORD timeout = IO_TIMEOUT_MS;
            setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
            setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

            if (tls) {
                m_ssl = std::make_unique<mbedtls_ssl_context>();
                mbedtls_ssl_init(m_ssl.get());
                mbedtls_ssl_setup(m_ssl.get(), tls);
                mbedtls_ssl_set_bio(m_ssl.get(), &m_socket, SocketSend, SocketRecv, nullptr);
            }
        }

        ~Connection() {
            if (m_ssl) {
                if (m_handshakeDone)
                    mbedtls_ssl_close_notify(m_ssl.get());
                mbedtls_ssl_free(m_ssl.get());
            }
            closesocket(m_socket);
        }

        bool Handshake() {
            if (!m_ssl)
                return true;

            int result = mbedtls_ssl_handshake(m_ssl.get());
            if (result != 0) {
                Log::Warn("[HTTP] TLS handshake failed: %s", TlsError(result).c_str());
                return false;
            }
            m_handshakeDone = true;
            return true;
        }

        // Bytes read, 0 at the end of the stream, negative on error.
        int Read(char* buffer, int size) {
            if (!m_ssl)
                return recv(m_socket, buffer, size, 0);

            int result = mbedtls_ssl_read(m_ssl.get(), (unsigned char*)buffer, (size_t)size);
            return result == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY ? 0 : result;
        }

        bool Write(const string& data) {
            for (size_t offset = 0; offset < data.size();) {
                const char* p = data.data() + offset;
                size_t left = data.size() - offset;
                int written = m_ssl ? mbedtls_ssl_write(m_ssl.get(), (const unsigned char*)p, left)
                                    : send(m_socket, p, (int)left, 0);
                if (written <= 0)
                    return false;
                offset += (size_t)written;
            }
            return true;
        }

    private:
        SOCKET m_socket;
        std::unique_ptr<mbedtls_ssl_context> m_ssl;
        bool m_handshakeDone = false;
    };

    bool ReadRequest(Connection& connection, HttpRequest& request) {
        string data;
        char buffer[4096];
        size_t headerEnd;

        while ((headerEnd = data.find("\r\n\r\n")) == string::npos) {
            int read = connection.Read(buffer, sizeof(buffer));
            if (read <= 0 || data.size() > MAX_HEADER_SIZE)
                return false;
            data.append(buffer, (size_t)read);
        }

        // request line: METHOD PATH VERSION
        string_view head(data.data(), headerEnd);
        string_view requestLine = head.substr(0, head.find("\r\n"));
        size_t space1 = requestLine.find(' ');
        size_t space2 = requestLine.find(' ', space1 + 1);
        if (space1 == string_view::npos || space2 == string_view::npos)
            return false;

        request.method = string(requestLine.substr(0, space1));
        request.path = string(requestLine.substr(space1 + 1, space2 - space1 - 1));

        // headers
        size_t lineStart = requestLine.size() + 2;
        while (lineStart < head.size()) {
            size_t lineEnd = head.find("\r\n", lineStart);
            string_view line = head.substr(lineStart, lineEnd - lineStart);
            size_t colon = line.find(':');
            if (colon != string_view::npos)
                request.headers[Strings::ToLower(Strings::Trim(line.substr(0, colon)))] =
                    string(Strings::Trim(line.substr(colon + 1)));
            if (lineEnd == string_view::npos)
                break;
            lineStart = lineEnd + 2;
        }

        size_t contentLength = (size_t)Strings::ToInt(request.GetHeader("content-length")).value_or(0);
        if (contentLength > MAX_BODY_SIZE)
            return false;

        request.body = data.substr(headerEnd + 4);
        while (request.body.size() < contentLength) {
            int read = connection.Read(buffer, sizeof(buffer));
            if (read <= 0)
                return false;
            request.body.append(buffer, (size_t)read);
        }
        request.body.resize(contentLength);
        return true;
    }

    const char* StatusText(int status) {
        switch (status) {
            case 200:
                return "OK";
            case 400:
                return "Bad Request";
            case 404:
                return "Not Found";
            default:
                return "Internal Server Error";
        }
    }

    void WriteResponse(Connection& connection, const HttpResponse& response) {
        string text = "HTTP/1.0 " + std::to_string(response.status) + " " + StatusText(response.status) + "\r\n";
        if (!response.body.empty())
            text += "Content-Type: text/html\r\n";
        text += "Content-Length: " + std::to_string(response.body.size()) + "\r\n";
        text += "Connection: close\r\n\r\n";
        text += response.body;
        connection.Write(text);
    }

}

string_view HttpRequest::GetHeader(const string& lowerCaseName) const {
    auto it = headers.find(lowerCaseName);
    return it != headers.end() ? string_view(it->second) : string_view();
}

struct HttpServer::Listener {
    uint16 port = 0;
    bool tls = false;
    SOCKET socket = INVALID_SOCKET;
    std::thread thread;
};

struct HttpServer::TlsConfig {
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context random;
    mbedtls_x509_crt certificate;
    mbedtls_pk_context key;
    mbedtls_ssl_config config;

    TlsConfig() {
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&random);
        mbedtls_x509_crt_init(&certificate);
        mbedtls_pk_init(&key);
        mbedtls_ssl_config_init(&config);
    }

    ~TlsConfig() {
        mbedtls_ssl_config_free(&config);
        mbedtls_pk_free(&key);
        mbedtls_x509_crt_free(&certificate);
        mbedtls_ctr_drbg_free(&random);
        mbedtls_entropy_free(&entropy);
    }
};

HttpServer::HttpServer(Handler handler) : m_handler(std::move(handler)) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

HttpServer::~HttpServer() {
    Stop();
    WSACleanup();
}

bool HttpServer::LoadCertificate(const string& certPath, const string& keyPath) {
    if (psa_crypto_init() != PSA_SUCCESS) {
        Log::Error("[HTTP] psa_crypto_init failed");
        return false;
    }

    std::vector<unsigned char> cert;
    std::vector<unsigned char> key;
    if (!ReadFile(certPath, cert) || !ReadFile(keyPath, key)) {
        Log::Error("[HTTP] Could not read %s / %s", certPath.c_str(), keyPath.c_str());
        return false;
    }
    // PEM parsing needs the terminating NUL inside the buffer
    cert.push_back(0);
    key.push_back(0);

    auto tls = std::make_unique<TlsConfig>();
    int result;
    if ((result = mbedtls_ctr_drbg_seed(&tls->random, mbedtls_entropy_func, &tls->entropy, nullptr, 0)) != 0 ||
        (result = mbedtls_x509_crt_parse(&tls->certificate, cert.data(), cert.size())) != 0 ||
        (result = mbedtls_pk_parse_key(&tls->key, key.data(), key.size(), nullptr, 0, mbedtls_ctr_drbg_random,
                                       &tls->random)) != 0 ||
        (result = mbedtls_ssl_config_defaults(&tls->config, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM,
                                              MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        Log::Error("[HTTP] TLS setup failed: %s", TlsError(result).c_str());
        return false;
    }

    mbedtls_ssl_conf_rng(&tls->config, mbedtls_ctr_drbg_random, &tls->random);
    if ((result = mbedtls_ssl_conf_own_cert(&tls->config, &tls->certificate, &tls->key)) != 0) {
        Log::Error("[HTTP] TLS setup failed: %s", TlsError(result).c_str());
        return false;
    }

    m_tls = std::move(tls);
    return true;
}

bool HttpServer::Listen(uint16 port, bool tls) {
    if (tls && !m_tls) {
        Log::Error("[HTTP] Port %u needs a certificate", port);
        return false;
    }

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (s == INVALID_SOCKET || bind(s, (sockaddr*)&addr, sizeof(addr)) != 0 || listen(s, SOMAXCONN) != 0) {
        Log::Error("[HTTP] Cannot listen on port %u (error %d); is another program using it?", port, WSAGetLastError());
        if (s != INVALID_SOCKET)
            closesocket(s);
        return false;
    }

    m_running = true;

    auto listener = std::make_unique<Listener>();
    listener->port = port;
    listener->tls = tls;
    listener->socket = s;
    listener->thread = std::thread(&HttpServer::Serve, this, std::ref(*listener));
    m_listeners.push_back(std::move(listener));
    return true;
}

void HttpServer::Stop() {
    m_running = false;
    for (auto& listener : m_listeners) {
        if (listener->thread.joinable())
            listener->thread.join();
        closesocket(listener->socket);
    }
    m_listeners.clear();
}

void HttpServer::Serve(Listener& listener) {
    while (m_running) {
        WSAPOLLFD poll{};
        poll.fd = listener.socket;
        poll.events = POLLIN;
        if (WSAPoll(&poll, 1, 200) <= 0)
            continue;

        SOCKET client = accept(listener.socket, nullptr, nullptr);
        if (client == INVALID_SOCKET)
            continue;

        Connection connection(client, listener.tls ? &m_tls->config : nullptr);
        HttpRequest request;
        if (connection.Handshake() && ReadRequest(connection, request))
            WriteResponse(connection, m_handler(request));
    }
}

// TLS certificate

namespace fs = std::filesystem;

namespace {

    constexpr const char* MKCERT_VERSION = "v1.4.4";

    const char* MkcertAsset() {
#if defined(_M_ARM64)
        return "windows-arm64.exe";
#else
        return "windows-amd64.exe";
#endif
    }

    // Runs a program without a console window and waits for it to exit.
    bool Run(const fs::path& exe, const std::vector<string>& args, const fs::path& workingDir) {
        std::wstring command = L"\"" + exe.wstring() + L"\"";
        for (const string& arg : args)
            command += L" \"" + Platform::ToWide(arg) + L"\"";

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                            workingDir.wstring().c_str(), &startup, &process))
            return false;

        WaitForSingleObject(process.hProcess, INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(process.hProcess, &exitCode);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return exitCode == 0;
    }

    bool DownloadMkcert(const fs::path& target) {
        string path = string("/FiloSottile/mkcert/releases/download/") + MKCERT_VERSION + "/mkcert-" + MKCERT_VERSION +
                      "-" + MkcertAsset();
        Log::Info("[SSL] Downloading mkcert from https://github.com%s", path.c_str());

        HttpClient http("https://github.com");
        std::error_code error;
        if (!http.DownloadTo(path, target.u8string()) || fs::file_size(target, error) == 0) {
            Log::Error("[SSL] mkcert download failed (status %u)", http.GetStatus());
            fs::remove(target, error);
            return false;
        }
        return true;
    }

}

bool Certificates::Ensure(const string& directory, Paths& out) {
    fs::path dir = fs::u8path(directory);
    fs::path cert = dir / "cert.pem";
    fs::path key = dir / "key.pem";
    out.certificate = cert.u8string();
    out.key = key.u8string();

    std::error_code error;
    if (fs::exists(cert, error) && fs::exists(key, error))
        return true;

    Log::Info("[SSL] Creating a certificate for the HTTPS server...");

    fs::path mkcert = dir / "mkcert.exe";
    if (!fs::exists(mkcert, error) && !DownloadMkcert(mkcert))
        return false;

    Run(mkcert, {"-install"}, dir);
    Run(mkcert, {"-cert-file", "cert.pem", "-key-file", "key.pem", "*.growtopia1.com", "*.growtopia2.com"}, dir);

    if (!fs::exists(cert, error) || !fs::exists(key, error)) {
        Log::Error("[SSL] mkcert did not create cert.pem / key.pem");
        return false;
    }

    Log::Info("[SSL] Certificate created.");
    return true;
}
