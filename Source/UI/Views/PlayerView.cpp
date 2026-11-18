#include "UI/ProxyUI/ProxyUI.h"

#include "Proxy/Proxy.h"
#include "UI/Icons.h"
#include "UI/Widgets/Widgets.h"

#include <cmath>

namespace {

    float Scale() { return ImGui::GetStyle().FontScaleDpi; }

}

void ProxyUI::DrawPlayer() {
    static const char* const TABS[] = {ICON_FA_USER "  Overview", ICON_FA_SCREWDRIVER_WRENCH "  Tools"};
    Widgets::Segmented("##player_tabs", &m_playerTab, TABS, 2);
    ImGui::Dummy(ImVec2(0, 2.0f * Scale()));

    if (m_playerTab == 0) {
        DrawPlayerOverview();
    } else {
        DrawFeatures();
    }
}

void ProxyUI::DrawPlayerOverview() {
    const GameState& state = m_proxy.State();
    const LocalPlayer& local = state.local;
    const bool known = local.netID != -1;
    const bool inWorld = known && state.world.IsLoaded();
    char text[96];

    const float gap = ImGui::GetStyle().ItemSpacing.x * 1.6f;
    const float width = ImGui::GetContentRegionAvail().x;
    const bool twoColumns = width > 620.0f * Scale();
    const float column = twoColumns ? std::floor((width - gap) * 0.5f) : width;

    Widgets::BeginCard("##player", ImVec2(column, 0), ICON_FA_USER, "Player");

    Widgets::KeyValue("Name", known && !local.name.empty() ? local.name.c_str() : "-");
    snprintf(text, sizeof(text), known ? "%d" : "-", local.netID);
    Widgets::KeyValue("Net ID", text);

    snprintf(text, sizeof(text), inWorld ? "%d, %d" : "-", (int)std::floor(local.pos.x / 32.0f),
             (int)std::floor(local.pos.y / 32.0f));
    Widgets::KeyValue("Position", text);

    if (state.inventory.GetSlots() > 0)
        snprintf(text, sizeof(text), "%zu / %u slots", state.inventory.GetItems().size(), state.inventory.GetSlots());
    else
        snprintf(text, sizeof(text), "-");
    Widgets::KeyValue("Inventory", text);

    Widgets::EndCard();

    if (twoColumns)
        ImGui::SameLine(0, gap);

    const World& world = state.world;
    Widgets::BeginCard("##player_world", ImVec2(column, 0), ICON_FA_EARTH_AMERICAS, "World");
    Widgets::KeyValue("Name", world.IsLoaded() ? world.GetName().c_str() : "-");

    const bool online = m_proxy.IsServerConnected();

    ImGui::Dummy(ImVec2(0, 4.0f * Scale()));
    Widgets::SectionLabel("Warp to world");
    ImGui::BeginDisabled(!online);
    float buttonWidth = 96.0f * Scale();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - buttonWidth - ImGui::GetStyle().ItemSpacing.x);
    bool submit = ImGui::InputTextWithHint("##warp", "World name, e.g. START", m_warpWorld, sizeof(m_warpWorld),
                                           ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsUppercase);
    ImGui::SameLine();
    submit |= Widgets::Button("Warp", Widgets::ButtonKind::Primary, ImVec2(buttonWidth, 0));
    ImGui::EndDisabled();
    if (submit && m_warpWorld[0] != '\0') {
        m_proxy.GetCommands().Warp(m_warpWorld);
        Log::Info("[UI] Warping to %s", m_warpWorld);
    }
    if (!online)
        Widgets::TextColored(Theme::TextFaint, Theme::FONT_SMALL, "Connect the game to the proxy first.");
    Widgets::EndCard();
}

void ProxyUI::DrawFeatures() {
    Widgets::BeginCard("##features", ImVec2(0, 0), ICON_FA_WAND_MAGIC_SPARKLES, "Toggles");

    ProxySettings& settings = m_proxy.Settings();
    GameState& state = m_proxy.State();

    if (Widgets::ToggleRow("##longpunch", "Long punch & click-to-walk",
                           "Hold left Shift for unlimited punch and build range. Shift + click a tile to walk there.",
                           &settings.longPunch))
        Log::Info("[UI] Long punch %s", settings.longPunch ? "enabled" : "disabled");

    ImGui::Dummy(ImVec2(0, 4.0f * Scale()));
    if (Widgets::ToggleRow("##zoom", "Infinite zoom", "Unlocks zooming out fully. Applies when you next enter a world.",
                           &settings.infiniteZoom))
        Log::Info("[UI] Infinite zoom %s", settings.infiniteZoom ? "enabled" : "disabled");

    ImGui::Dummy(ImVec2(0, 4.0f * Scale()));
    if (Widgets::ToggleRow("##paint_entrances", "Paint entrances",
                           "Shows house, VIP and friends entrances painted in the game: green if you can pass, red if "
                           "you can't. Only you see it. Applies when you next enter a world.",
                           &settings.paintEntrances))
        Log::Info("[UI] Paint entrances %s", settings.paintEntrances ? "enabled" : "disabled");

    ImGui::Dummy(ImVec2(0, 4.0f * Scale()));
    bool inWorld = m_proxy.IsClientConnected() && state.world.IsLoaded();
    bool antigravity = state.antigravityEnabled;
    ImGui::BeginDisabled(!inWorld);
    if (Widgets::ToggleRow("##antigravity", "Antigravity", "Client-side antigravity generator. Same as /antigravity.",
                           &antigravity))
        m_proxy.GetCommands().ToggleAntigravity();
    ImGui::EndDisabled();

    Widgets::EndCard();
}
