#include "Core/Config.h"
#include "Core/Log.h"
#include "Core/Platform.h"
#include "Proxy/Proxy.h"
#include "Network/Http.h"
#include "Network/ServerData.h"
#include "UI/PacketCapture.h"
#include "UI/ProxyUI.h"
#include "UI/Window.h"

#include <enet/enet.h>
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>

namespace {

    std::atomic<bool> s_running{true};

    BOOL WINAPI OnConsoleSignal(DWORD signal) {
        if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_CLOSE_EVENT) {
            s_running = false;
            return TRUE;
        }
        return FALSE;
    }

    // The executable is a GUI application; --headless gets a console (the parent's, or a new one).
    void OpenConsole() {
        if (!AttachConsole(ATTACH_PARENT_PROCESS))
            AllocConsole();

        FILE* stream = nullptr;
        freopen_s(&stream, "CONOUT$", "w", stdout);
        freopen_s(&stream, "CONOUT$", "w", stderr);
        SetConsoleCtrlHandler(OnConsoleSignal, TRUE);
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

    void RunHeadless(Proxy& proxy) {
        Log::Info("Proxy is running on %s:%u. Press Ctrl+C to stop.", Config::LOCAL_HOST, Config::LOCAL_ENET_PORT);
        while (s_running)
            proxy.Poll(1);
    }

    // The proxy runs on a network thread; the UI runs on this one. Everything the proxy owns is guarded by one
    // mutex: the network thread holds it around Poll, the UI around building each frame. Waiting for packets
    // happens outside the lock so the UI never waits on the network.
    bool RunWithUI(Proxy& proxy) {
        Window window;
        if (!window.Create("GrowProxy", 1040, 660)) {
            Log::Error("[UI] Could not create the window");
            return false;
        }

        std::mutex mutex;
        PacketCapture capture;
        proxy.SetPacketObserver([&capture](const PacketEvent& event) { capture.Add(event); });

        std::atomic<bool> networkRunning{true};
        std::thread network([&]() {
            while (networkRunning) {
                proxy.WaitForTraffic(1);
                std::lock_guard<std::mutex> lock(mutex);
                proxy.Poll(0);
            }
        });

        Log::Info("Proxy is running on %s:%u", Config::LOCAL_HOST, Config::LOCAL_ENET_PORT);

        ProxyUI ui(proxy, capture);
        window.SetDrawCallback([&]() {
            std::lock_guard<std::mutex> lock(mutex);
            ui.Draw();
        });
        while (s_running && !window.ShouldClose())
            window.Frame();

        networkRunning = false;
        network.join();
        proxy.SetPacketObserver(nullptr);
        window.Destroy();
        return true;
    }

}

int main(int argc, char** argv) {
    bool headless = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--headless") == 0)
            headless = true;
    }

    if (headless)
        OpenConsole();
    Log::Init();

    if (enet_initialize() != 0) {
        Log::Error("Failed to initialize ENet");
        return 1;
    }

    ServerData serverData;
    HttpServer http([&serverData](const HttpRequest& request) { return serverData.Handle(request); });
    if (!StartHttpServer(http)) {
        Log::Error("The HTTP server could not start; the game client will not find the proxy");
        if (!headless)
            MessageBoxA(nullptr,
                        "The HTTP server could not start on port 80/443.\n\nAnother program may be using these ports, "
                        "or GrowProxy needs to run as administrator.",
                        "GrowProxy", MB_ICONERROR | MB_OK);
        enet_deinitialize();
        return 1;
    }

    Proxy proxy(serverData);
    if (!proxy.Start(Config::LOCAL_HOST, Config::LOCAL_ENET_PORT)) {
        Log::Error("Cannot listen on %s:%u", Config::LOCAL_HOST, Config::LOCAL_ENET_PORT);
        if (!headless)
            MessageBoxA(nullptr, "Cannot listen on the local proxy port. Is GrowProxy already running?", "GrowProxy",
                        MB_ICONERROR | MB_OK);
        http.Stop();
        enet_deinitialize();
        return 1;
    }

    if (headless)
        RunHeadless(proxy);
    else if (!RunWithUI(proxy))
        MessageBoxA(nullptr, "Could not create the window (OpenGL 3.0 is required).\n\nRun GrowProxy --headless for "
                             "the console version.",
                    "GrowProxy", MB_ICONERROR | MB_OK);

    Log::Info("Shutting down...");
    proxy.Stop();
    http.Stop();
    enet_deinitialize();
    return 0;
}
