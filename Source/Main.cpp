#include "Core/Config.h"
#include "Core/Log.h"
#include "Core/Platform.h"
#include "Proxy/Proxy.h"
#include "Network/Http.h"
#include "Network/ServerData.h"

#include <enet/enet.h>
#include <windows.h>

#include <atomic>

namespace {

    std::atomic<bool> s_running{true};

    BOOL WINAPI OnConsoleSignal(DWORD signal) {
        if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_CLOSE_EVENT) {
            s_running = false;
            return TRUE;
        }
        return FALSE;
    }

    bool StartHttpServer(HttpServer& http) {
        Certificates::Paths certificate;
        bool httpsReady = Certificates::Ensure(Platform::GetExecutableDir(), certificate) &&
                          http.LoadCertificate(certificate.certificate, certificate.key);

        bool httpOk = http.Listen(Config::HTTP_PORT, false);
        bool httpsOk = httpsReady && http.Listen(Config::HTTPS_PORT, true);
        if (!httpsReady)
            Log::Warn("[HTTP] HTTPS is unavailable without a certificate; only port %u is served", Config::HTTP_PORT);

        if (httpOk || httpsOk)
            Log::Info("[HTTP] Serving server_data.php on%s%s", httpOk ? " :80" : "", httpsOk ? " :443" : "");
        return httpOk || httpsOk;
    }

}

int main(int argc, char** argv) {
    Log::Init();
    SetConsoleCtrlHandler(OnConsoleSignal, TRUE);

    if (enet_initialize() != 0) {
        Log::Error("Failed to initialize ENet");
        return 1;
    }

    ServerData serverData;
    HttpServer http([&serverData](const HttpRequest& request) { return serverData.Handle(request); });
    if (!StartHttpServer(http)) {
        Log::Error("The HTTP server could not start; the game client will not find the proxy");
        enet_deinitialize();
        return 1;
    }

    Proxy proxy(serverData);
    if (!proxy.Start(Config::LOCAL_HOST, Config::LOCAL_ENET_PORT)) {
        Log::Error("Cannot listen on %s:%u", Config::LOCAL_HOST, Config::LOCAL_ENET_PORT);
        enet_deinitialize();
        return 1;
    }

    Log::Info("Proxy is running on %s:%u. Press Ctrl+C to stop.", Config::LOCAL_HOST, Config::LOCAL_ENET_PORT);

    while (s_running)
        proxy.Poll(1);

    Log::Info("Shutting down...");
    proxy.Stop();
    http.Stop();
    enet_deinitialize();
    return 0;
}
