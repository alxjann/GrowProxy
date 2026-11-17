#pragma once

#include "Features/Commands.h"
#include "Game/GameState.h"
#include "Network/ServerData.h"
#include "Packets/Packets.h"
#include "Packets/VariantList.h"

#include <functional>
#include <map>
#include <vector>

struct _ENetHost;
struct _ENetPeer;
struct _ENetEvent;

// Where a packet created by the proxy goes: the Growtopia client or the real game server.
enum Target { Client, Server };

// Features that can be switched on and off at runtime (from the UI).
struct ProxySettings {
    bool longPunch = true;    // hold left shift for unlimited punch/build range, shift + click walks there
    bool infiniteZoom = true; // patches mstate|1 into the local player's OnSpawn
    bool paintEntrances = false; // shows entrances painted in the game: green if you can pass, red if not
};

// One packet passing through the proxy, reported to the packet observer. data/size are only valid during the call.
struct PacketEvent {
    Target to = Server;     // the side that receives the packet
    bool fromProxy = false; // created by the proxy instead of forwarded
    bool blocked = false;   // dropped by the proxy (or not sent because that side is not connected)
    const uint8* data = nullptr;
    size_t size = 0;
};

// Man-in-the-middle between the game client and the real game server.
//
// The client connects to a local ENet server; the proxy then connects to the real server and forwards packets
// both ways. Every packet is inspected on the way (OnPacketFromServer / OnPacketFromClient), which keeps GameState
// up to date, may change or block it, and implements the proxy's own features (commands, long punch, pathfinding).
// When the server moves the client to another sub-server (OnSendToServer) the proxy reconnects to the new target
// when the client does.
//
// Not thread-safe: everything, including Schedule() callbacks, runs on the thread calling Poll().
class Proxy {
public:
    explicit Proxy(const ServerData& serverData);
    ~Proxy();

    bool Start(const char* host, int port);
    void Stop();

    // Services the network once (waiting up to timeoutMs) and runs due scheduled tasks.
    void Poll(int timeoutMs);

    // Blocks until either connection has incoming data or timeoutMs passes, without touching any proxy state.
    // Lets a network thread wait without holding the lock that guards the proxy, then Poll(0) under it.
    // Must be called from the thread that calls Poll().
    void WaitForTraffic(int timeoutMs) const;

    // Drops both connections on the next Poll(); the game client will reconnect through server_data.php.
    void Disconnect();

    // Runs a task on the network thread after a delay.
    void Schedule(int delayMs, std::function<void()> task);

    // The client will reconnect to the proxy; connect it to this server instead of the one from server_data.php.
    void RedirectNextConnection(const Endpoint& server);

    GameState& State() { return m_state; }
    const GameState& State() const { return m_state; }
    Commands& GetCommands() { return m_commands; }
    ProxySettings& Settings() { return m_settings; }

    bool IsClientConnected() const { return m_client != nullptr; }
    bool IsServerConnected() const { return m_serverConnected; }
    bool IsConnectingToServer() const { return m_server != nullptr && !m_serverConnected; }
    const Endpoint& GetServerEndpoint() const { return m_serverEndpoint; } // last server connected to
    int GetServerPing() const; // round trip time in ms, 0 when not connected

    // Called on the network thread for every packet received, forwarded, blocked or created by the proxy.
    void SetPacketObserver(std::function<void(const PacketEvent&)> observer) { m_observer = std::move(observer); }

    // Packets created by the proxy itself (reliable, channel 0).
    // target: Client sends to the Growtopia client, Server to the game server.
    void Send(Target target, const GameUpdatePacket& packet, const void* extraData = nullptr, uint32 extraSize = 0);
    void Send(Target target, MessageType type, string_view text);
    void Send(Target target, const VariantList& call, int netID = -1, int delayMs = 0);

    void ConsoleMessage(const string& text);
    void TextOverlay(const string& text);
    void ShowDialog(const string& dialog);

private:
    struct PendingPacket {
        std::vector<uint8> data;
        uint8 channel;
        uint32 flags;
    };

    // Return false to drop the packet instead of forwarding it.
    bool OnPacketFromServer(const PacketView& packet);
    bool OnPacketFromClient(const PacketView& packet);

    // Sends raw packet bytes to the client or the server; does nothing (returns false) while that side is not
    // connected.
    bool SendPacket(Target target, const uint8* data, size_t size, int channel, int flags);
    void SendOwnPacket(Target target, const std::vector<uint8>& data);
    void Notify(Target to, bool fromProxy, bool blocked, const uint8* data, size_t size);

    void HandleClientEvent(_ENetEvent& event);
    void HandleServerEvent(_ENetEvent& event);
    void ConnectToServer(const Endpoint& server);
    void CloseServerConnection();
    void DisconnectClient();
    void RunDueTasks();
    void UpdateLongPunch();

    bool GetEntrancePaint(const Tile& tile, int x, int y, uint16& outFlags) const;

    const ServerData& m_serverData;
    GameState m_state;
    Commands m_commands;
    ProxySettings m_settings;
    std::function<void(const PacketEvent&)> m_observer;

    _ENetHost* m_clientHost = nullptr; // local server the game client connects to
    _ENetHost* m_serverHost = nullptr; // our connection to the real server
    _ENetPeer* m_client = nullptr;
    _ENetPeer* m_server = nullptr;

    bool m_serverConnected = false;
    Endpoint m_serverEndpoint;
    uint64 m_connectStartedMs = 0;
    bool m_gazetteBannerInstalled = false; // banner copied into the game folder (see OnDialogRequest)
    std::vector<PendingPacket> m_pendingToServer; // client packets received while still connecting

    bool m_redirecting = false;
    Endpoint m_redirectTarget;

    int m_userID = -1;

    std::multimap<uint64, std::function<void()>> m_tasks; // by due time
};
