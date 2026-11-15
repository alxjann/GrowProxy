#include "UI/ProxyUI.h"

#include "Features/Pathfinding/Pathfinding.h"
#include "Proxy/Proxy.h"
#include "UI/Icons.h"
#include "UI/PacketCapture.h"
#include "UI/Widgets.h"
#include "Utils/Strings.h"

#include <imgui_internal.h>

#include <cmath>

namespace {

    float Scale() { return ImGui::GetStyle().FontScaleDpi; }

}

ProxyUI::ProxyUI(Proxy& proxy, PacketCapture& capture) : m_proxy(proxy), m_capture(capture) {}

void ProxyUI::Draw() {
    PullLogs();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("##root", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar);

    // sidebar
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::Sidebar);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f * Scale(), 18.0f * Scale()));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::BeginChild("##sidebar", ImVec2(Theme::SIDEBAR_WIDTH * Scale(), 0), ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    DrawSidebar();
    ImVec2 sidebarMax = ImGui::GetWindowPos() + ImGui::GetWindowSize();
    ImGui::EndChild();
    ImGui::GetWindowDrawList()->AddLine(ImVec2(sidebarMax.x - 1, viewport->WorkPos.y),
                                        ImVec2(sidebarMax.x - 1, sidebarMax.y), Theme::U32(Theme::Border));

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::BeginChild("##main", ImVec2(0, 0), 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28.0f * Scale(), 12.0f * Scale()));
        ImGui::BeginChild("##header", ImVec2(0, Theme::HEADER_HEIGHT * Scale()), ImGuiChildFlags_AlwaysUseWindowPadding,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();
        DrawHeader();
        ImVec2 headerMin = ImGui::GetWindowPos();
        ImVec2 headerMax = headerMin + ImGui::GetWindowSize();
        ImGui::EndChild();
        ImGui::GetWindowDrawList()->AddLine(ImVec2(headerMin.x, headerMax.y - 1), ImVec2(headerMax.x, headerMax.y - 1),
                                            Theme::U32(Theme::Border));

        // pages that lay themselves out to the window (world, packets, console) do not scroll
        bool fills = m_page == Page::World || m_page == Page::Packets || m_page == Page::Console ||
                     m_page == Page::Items;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28.0f * Scale(), 22.0f * Scale()));
        ImGui::BeginChild("##page", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding,
                          fills ? ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse : 0);
        ImGui::PopStyleVar();

        switch (m_page) {
            case Page::Info:
                DrawInfo();
                break;
            case Page::Player:
                DrawPlayer();
                break;
            case Page::World:
                DrawWorld();
                break;
            case Page::Inventory:
                DrawInventory();
                break;
            case Page::Items:
                DrawItems();
                break;
            case Page::Packets:
                DrawPackets();
                break;
            case Page::Console:
                DrawConsole();
                break;
            default:
                break;
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();

    ImGui::End();
}

void ProxyUI::DrawSidebar() {
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 6.0f * Scale());
    Widgets::TextColored(Theme::Text, Theme::FONT_LARGE, "GrowProxy");
    ImGui::Dummy(ImVec2(0, 14.0f * Scale()));

    auto nav = [&](Page page, const char* icon, const char* label) {
        if (Widgets::NavItem(icon, label, m_page == page))
            m_page = page;
    };

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f * Scale(), 3.0f * Scale()));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 6.0f * Scale());
    Widgets::SectionLabel("Session");
    nav(Page::Info, ICON_FA_CIRCLE_INFO, "Info");
    nav(Page::Player, ICON_FA_USER, "Player");
    nav(Page::World, ICON_FA_EARTH_AMERICAS, "World");
    nav(Page::Inventory, ICON_FA_BOX_OPEN, "Inventory");

    ImGui::Dummy(ImVec2(0, 14.0f * Scale()));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 6.0f * Scale());
    Widgets::SectionLabel("Data");
    nav(Page::Items, ICON_FA_CUBES, "Items");
    nav(Page::Packets, ICON_FA_ARROW_RIGHT_ARROW_LEFT, "Packets");
    nav(Page::Console, ICON_FA_TERMINAL, "Console");
    ImGui::PopStyleVar();
}

void ProxyUI::DrawHeader() {
    const GameState& state = m_proxy.State();
    const World& world = state.world;

    string title;
    string subtitle;
    char buffer[160];
    switch (m_page) {
        case Page::Info:
            title = "Info";
            subtitle = "Proxy connection and traffic";
            break;
        case Page::Player:
            title = "Player";
            subtitle = state.local.name.empty() ? "Enter a world in the game to see your player" : state.local.name;
            break;
        case Page::World:
            if (world.IsLoaded()) {
                title = world.GetName();
                snprintf(buffer, sizeof(buffer), "%d x %d tiles  \xC2\xB7  %zu players  \xC2\xB7  %zu floating items",
                         world.GetWidth(), world.GetHeight(), state.players.size(),
                         world.GetDroppedItems().size());
                subtitle = buffer;
            } else {
                title = "World";
                subtitle = "Enter a world in the game to see it here";
            }
            break;
        case Page::Inventory:
            title = "Inventory";
            snprintf(buffer, sizeof(buffer), "%zu items  \xC2\xB7  %u slots", state.inventory.GetItems().size(),
                     state.inventory.GetSlots());
            subtitle = buffer;
            break;
        case Page::Items:
            title = "Item database";
            if (state.items.Count() > 0)
                snprintf(buffer, sizeof(buffer), "%s items  \xC2\xB7  items.dat v%u",
                         Widgets::FormatNumber(state.items.Count()).c_str(), state.items.Version());
            else
                snprintf(buffer, sizeof(buffer), "items.dat is loaded when the game downloads it");
            subtitle = buffer;
            break;
        case Page::Packets:
            title = "Packet inspector";
            snprintf(buffer, sizeof(buffer), "%s packets in buffer  \xC2\xB7  %s",
                     Widgets::FormatNumber(m_capture.Packets().size()).c_str(),
                     m_capture.recording ? "recording" : "paused");
            subtitle = buffer;
            break;
        case Page::Console:
            title = "Console";
            subtitle = "Proxy log and commands";
            break;
        default:
            break;
    }

    Widgets::TextColored(Theme::Text, Theme::FONT_TITLE, "%s", title.c_str());
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 7.0f * Scale());
    Widgets::TextColored(Theme::TextMuted, Theme::FONT_SMALL, "%s", subtitle.c_str());
}

void ProxyUI::PullLogs() {
    size_t before = m_console.entries.size();
    m_console.lastSequence = Log::Fetch(m_console.lastSequence, m_console.entries);

    constexpr size_t MAX_ENTRIES = 4000;
    if (m_console.entries.size() > MAX_ENTRIES)
        m_console.entries.erase(m_console.entries.begin(),
                                m_console.entries.begin() + (m_console.entries.size() - MAX_ENTRIES));

    if (m_console.entries.size() != before && m_console.autoScroll)
        m_console.scrollToBottom = true;
}

void ProxyUI::WalkTo(int32 x, int32 y) {
    if (!m_proxy.IsServerConnected() || !m_proxy.State().world.IsLoaded()) {
        Log::Warn("[UI] Cannot walk: not in a world");
        return;
    }
    ::WalkTo(m_proxy, {x, y});
}
