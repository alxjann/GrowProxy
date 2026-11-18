#include "UI/ProxyUI/ProxyUI.h"

#include "Core/Config.h"
#include "Proxy/Proxy.h"
#include "UI/Icons.h"
#include "UI/Widgets/Widgets.h"

namespace {

    float Scale() { return ImGui::GetStyle().FontScaleDpi; }

}

void ProxyUI::DrawInfo() {
    char text[128];

    // connection
    Widgets::BeginCard("##connection", ImVec2(0, 0), ICON_FA_PLUG, "Connection");
    {
        bool client = m_proxy.IsClientConnected();
        bool server = m_proxy.IsServerConnected();
        bool connecting = m_proxy.IsConnectingToServer();
        const Endpoint& endpoint = m_proxy.GetServerEndpoint();

        snprintf(text, sizeof(text), "%s:%d", Config::LOCAL_HOST, Config::LOCAL_ENET_PORT);
        Widgets::KeyValue("Proxy address", text);
        Widgets::KeyValue("Game client", client ? "Connected" : "Waiting", client ? Theme::Accent : Theme::TextMuted);
        Widgets::KeyValue("Game server", server ? "Connected" : (connecting ? "Connecting" : "Offline"),
                          server ? Theme::Accent : Theme::TextMuted);
        if (endpoint.host.empty())
            snprintf(text, sizeof(text), "-");
        else
            snprintf(text, sizeof(text), "%s:%d", endpoint.host.c_str(), endpoint.port);
        Widgets::KeyValue("Server address", text);
        snprintf(text, sizeof(text), server ? "%d ms" : "-", m_proxy.GetServerPing());
        Widgets::KeyValue("Ping", text);

        const ItemDatabase& items = m_proxy.State().items;
        if (items.Count() > 0)
            snprintf(text, sizeof(text), "%s items (v%d)", Widgets::FormatNumber(items.Count()).c_str(), items.Version());
        else
            snprintf(text, sizeof(text), "Not loaded");
        Widgets::KeyValue("Item database", text);

        ImGui::Dummy(ImVec2(0, 4.0f * Scale()));
        ImGui::BeginDisabled(!client && !connecting && !server);
        if (Widgets::Button(ICON_FA_POWER_OFF "  Disconnect", Widgets::ButtonKind::Danger,
                            ImVec2(ImGui::GetContentRegionAvail().x, 34.0f * Scale())))
            m_proxy.Disconnect();
        Widgets::Tooltip("Drops the client and server connections; the game reconnects by itself");
        ImGui::EndDisabled();
    }
    Widgets::EndCard();
}
