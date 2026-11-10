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

    bool Start(const char* host, uint16 port);
    void Stop();

    // Services the network once (waiting up to timeoutMs) and runs due scheduled tasks.
    void Poll(uint32 timeoutMs);

    // Runs a task on the network thread after a delay.
    void Schedule(uint32 delayMs, std::function<void()> task);

    // The client will reconnect to the proxy; connect it to this server instead of the one from server_data.php.
    void RedirectNextConnection(const Endpoint& server);

    GameState& State() { return m_state; }

    // Packets created by the proxy itself (reliable, channel 0).
    // target: Client sends to the Growtopia client, Server to the game server.
    void Send(Target target, const GameUpdatePacket& packet, const void* extraData = nullptr, uint32 extraSize = 0);
    void Send(Target target, MessageType type, string_view text);
    void Send(Target target, const VariantList& call, int32 netID = -1, uint32 delayMs = 0);

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

    // Sends raw packet bytes to the client or the server; does nothing while that side is not connected.
    void SendPacket(Target target, const uint8* data, size_t size, uint8 channel, uint32 flags);


    void HandleClientEvent(_ENetEvent& event);
    void HandleServerEvent(_ENetEvent& event);
    void ConnectToServer(const Endpoint& server);
    void CloseServerConnection();
    void DisconnectClient();
    void RunDueTasks();

    const ServerData& m_serverData;
    GameState m_state;
    Commands m_commands;

    _ENetHost* m_clientHost = nullptr; // local server the game client connects to
    _ENetHost* m_serverHost = nullptr; // our connection to the real server
    _ENetPeer* m_client = nullptr;
    _ENetPeer* m_server = nullptr;

    bool m_serverConnected = false;
    uint64 m_connectStartedMs = 0;
    std::vector<PendingPacket> m_pendingToServer; // client packets received while still connecting

    bool m_redirecting = false;
    Endpoint m_redirectTarget;

    std::multimap<uint64, std::function<void()>> m_tasks; // by due time
};
