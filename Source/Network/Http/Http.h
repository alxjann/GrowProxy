#pragma once

#include "Core/Types.h"

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <thread>
#include <vector>

// HTTP client

// Blocking HTTP(S) client (WinHTTP). Certificate errors are ignored; redirects are followed.
class HttpClient {
public:
    // "host", "host:port", "http://host[:port]" or "https://host[:port]". Defaults to HTTPS.
    explicit HttpClient(string_view server);

    void SetHeader(const string& name, const string& value) { m_headers[name] = value; }

    // Appends a form field to the request body (application/x-www-form-urlencoded).
    void AddFormField(string_view name, string_view value);

    bool Get(const string& path);
    bool Post(const string& path);

    // Writes the response body to a file instead of GetBody().
    bool DownloadTo(const string& path, const string& filePath);

    // HTTP status; 0 if no response was received.
    uint16 GetStatus() const { return m_status; }
    const string& GetBody() const { return m_body; }

private:
    bool Send(const wchar_t* method, const string& path, const string& filePath = "");

    string m_host;
    uint16 m_port = 443;
    bool m_secure = true;
    std::map<string, string> m_headers;
    string m_form;
    uint16 m_status = 0;
    string m_body;
};

// HTTP server

struct HttpRequest {
    string method;
    string path;
    std::map<string, string> headers; // lower-case names
    string body;

    string_view GetHeader(const string& lowerCaseName) const;
};

struct HttpResponse {
    int status = 200;
    string body;
};

// Minimal HTTP/1.0 server on several ports, optionally with TLS. Each port is served by its own thread, one
// connection at a time; every response closes the connection.
class HttpServer {
public:
    using Handler = std::function<HttpResponse(const HttpRequest&)>;

    explicit HttpServer(Handler handler);
    ~HttpServer();

    // Loads the TLS certificate used by ports added with tls = true.
    bool LoadCertificate(const string& certPath, const string& keyPath);

    // Binds a port on all interfaces and starts serving it.
    bool Listen(uint16 port, bool tls);

    void Stop();

private:
    struct Listener;
    struct TlsConfig;

    void Serve(Listener& listener);

    Handler m_handler;
    std::unique_ptr<TlsConfig> m_tls;
    std::vector<std::unique_ptr<Listener>> m_listeners;
    std::atomic<bool> m_running{false};
};

// TLS certificate

// TLS certificate for the game's server_data.php domains, created with mkcert.
namespace Certificates {
    struct Paths {
        string certificate;
        string key;
    };

    // Makes sure cert.pem / key.pem exist in the directory: downloads mkcert if needed, installs its local CA
    // (Windows may ask for confirmation the first time) and generates a certificate for *.growtopia1/2.com.
    bool Ensure(const string& directory, Paths& out);
}
