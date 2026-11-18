#pragma once

#include "Core/Log.h"
#include "Core/Types.h"
#include "UI/TextureCache/TextureCache.h"

#include <imgui.h>

#include <vector>

class Proxy;
class PacketCapture;
struct CapturedPacket;

// The proxy's window contents: a sidebar with pages for the proxy, the player, the world, the inventory, the item
// database, captured packets and the log.
//
// Draw() reads and changes the proxy directly, so it must run while holding the lock that guards the proxy
// (the same one the network thread holds around Proxy::Poll).
class ProxyUI {
public:
    ProxyUI(Proxy& proxy, PacketCapture& capture);

    // Builds the whole UI for this frame (between ImGui::NewFrame and ImGui::Render).
    void Draw();

private:
    enum class Page { Info, Player, World, Inventory, Items, Packets, Console, Count };

    void DrawSidebar();
    void DrawHeader();

    // Pages (one source file each under UI/Views).
    void DrawInfo();
    void DrawPlayer();
    void DrawWorld();
    void DrawInventory();
    void DrawItems();
    void DrawPackets();
    void DrawConsole();

    // Player parts.
    void DrawPlayerOverview();
    void DrawFeatures();

    // World parts.
    void DrawWorldToolbar();
    void DrawWorldMap(ImVec2 size);
    void DrawWorldInspector(ImVec2 size);
    void DrawTileTooltip(int32 x, int32 y);
    void DrawPlayerList();
    void DrawFloatingList();
    void DrawBlockList();

    // Packet parts.
    void DrawPacketToolbar();
    void DrawPacketTable(ImVec2 size);
    void DrawPacketDetails(ImVec2 size);
    bool PacketVisible(const CapturedPacket& packet) const;

    void PullLogs();
    void WalkTo(int32 x, int32 y);

    Proxy& m_proxy;
    PacketCapture& m_capture;
    Page m_page = Page::Info;
    int m_playerTab = 0; // 0 overview, 1 tools

    char m_warpWorld[32] = "";

    struct WorldView {
        float zoom = 1.0f;
        ImVec2 offset{0, 0}; // canvas pixels from the canvas corner to tile (0, 0)
        float lastTile = 0.0f; // tile size in pixels last frame
        bool needsFit = true;
        string worldName; // refit when this changes
        int32 hoverX = -1, hoverY = -1;
        int32 selectedX = -1, selectedY = -1;
        bool dragging = false;
        bool showTextures = true; // the game's block textures instead of flat colors
        bool showBackground = true;
        bool showDropped = false;
        bool showPlayers = true;
        bool showAccess = false;
        bool showEntrances = false; // colors house/VIP/friends entrances by whether you can pass
        bool showTileInfo = false; // hover tooltip
        int tab = 0; // 0 overview, 1 map
        int inspectorTab = 0; // 0 players, 1 floating, 2 blocks
        char search[64] = "";
    } m_world;

    TextureCache m_textures;

    char m_inventorySearch[64] = "";

    struct ItemsView {
        char search[64] = "";
        string lastQuery;
        size_t lastCount = (size_t)-1;
        std::vector<int32> results;
        int32 selected = -1;
    } m_items;

    struct PacketsView {
        int direction = 0; // 0 all, 1 to server, 2 to client
        bool hideNoise = true;
        bool autoScroll = true;
        uint64 selected = 0; // packet number, 0 = none
        int detailTab = 0;
        std::vector<int32> visible; // indices into the capture, rebuilt every frame
    } m_packets;

    struct ConsoleView {
        std::vector<Log::Entry> entries;
        uint64 lastSequence = 0;
        bool showInfo = true, showWarn = true, showError = true;
        bool autoScroll = true;
        bool scrollToBottom = false;
    } m_console;
};
