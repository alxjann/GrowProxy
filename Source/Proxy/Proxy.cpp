#include "Proxy/Proxy.h"

#include "Core/Config.h"
#include "Core/Log.h"
#include "Core/Platform.h"
#include "Features/Pathfinding/Pathfinding.h"
#include "Game/World/TileExtras.h"
#include "Packets/TextPacket.h"
#include "Utils/Hash.h"
#include "Utils/Strings.h"
#include "Utils/Zlib.h"

#include <enet/enet.h>
#include <windows.h>

constexpr size_t CHANNELS = 2;
constexpr size_t MAX_CLIENTS = 32;

Proxy::Proxy(const ServerData& serverData) : m_serverData(serverData), m_commands(*this) {}

Proxy::~Proxy() {
    Stop();
}

// Sending

void Proxy::SendPacket(Target target, const uint8* data, size_t size, uint8 channel, uint32 flags) {
    // the server only gets packets once the connection is up
    ENetPeer* peer = target == Client ? m_client : (m_serverConnected ? m_server : nullptr);
    if (!peer)
        return;

    ENetPacket* packet = enet_packet_create(data, size, flags);
    if (packet && enet_peer_send(peer, channel, packet) != 0)
        enet_packet_destroy(packet);
}

void Proxy::Send(Target target, const GameUpdatePacket& packet, const void* extraData, uint32 extraSize) {
    std::vector<uint8> data = MakeGamePacket(packet, extraData, extraSize);
    SendPacket(target, data.data(), data.size(), 0, ENET_PACKET_FLAG_RELIABLE);
}

void Proxy::Send(Target target, MessageType type, string_view text) {
    std::vector<uint8> data = MakeTextPacket(type, text);
    SendPacket(target, data.data(), data.size(), 0, ENET_PACKET_FLAG_RELIABLE);
}

void Proxy::Send(Target target, const VariantList& call, int32 netID, uint32 delayMs) {
    GameUpdatePacket packet;
    packet.type = PACKET_CALL_FUNCTION;
    packet.netID = netID;
    packet.value = (int32)delayMs;

    std::vector<uint8> data = call.Serialize();
    Send(target, packet, data.data(), (uint32)data.size());
}

void Proxy::ConsoleMessage(const string& text) {
    Send(Client, VariantList{"OnConsoleMessage", text});
}

void Proxy::TextOverlay(const string& text) {
    Send(Client, VariantList{"OnTextOverlay", text});
}

void Proxy::ShowDialog(const string& dialog) {
    Send(Client, VariantList{"OnDialogRequest", dialog});
}

// Server -> client

bool Proxy::OnPacketFromServer(const PacketView& packet) {
    switch (packet.Type()) {
        case MESSAGE_GENERIC_TEXT:
        case MESSAGE_GAME_MESSAGE: {
            if (packet.Text().find("action|quit") != string_view::npos) // also matches action|quit_to_exit
                m_state.LeaveWorld(true);
            return true;
        }

        case MESSAGE_GAME_PACKET:
            break;

        default:
            return true;
    }

    GameUpdatePacket game;
    const uint8* extra = nullptr;
    if (!packet.GetGamePacket(game, &extra))
        return true;

    switch (game.type) {
        case PACKET_CALL_FUNCTION: {
            VariantList call;
            if (!extra || !call.Parse(extra, game.extraDataSize))
                return true;

            switch (Hash(call.GetString(0))) {
                // The server moves the client to another sub-server: point the client at the proxy again and
                // remember the real target for when it reconnects.
                case "OnSendToServer"_hash: {
                    if (call.Size() < 5)
                        return true;

                    uint16 port = (uint16)call.GetInt(1);
                    string address = call.GetString(4); // "ip|doorID|uuid"; only the ip is replaced
                    size_t bar = address.find('|');
                    string ip = address.substr(0, bar);
                    string rest = bar != string::npos ? address.substr(bar) : "";

                    Log::Info("[Proxy] Server is moving the client to %s:%u", ip.c_str(), port);
                    RedirectNextConnection({ip, port});

                    call[1] = (int32)Config::LOCAL_ENET_PORT;
                    call[4] = Config::LOCAL_HOST + rest;

                    std::vector<uint8> data = call.Serialize();
                    Send(Client, game, data.data(), (uint32)data.size());
                    return false;
                }

                // If our items.dat is not the server's, report hash 0 so the client asks for items.dat and we
                // get to read it.
                case "OnSuperMainStartAcceptLogonFB211131d"_hash:
                case "OnSuperMainStartAcceptLogonFB211131dd"_hash:
                case "OnSuperMainStartAcceptLogonFB211131ddf"_hash:
                case "OnSuperMainStartAcceptLogonHrdxs47254722215a"_hash: {
                    if (call.Size() < 2)
                        return true;

                    uint32 serverHash = call.GetUInt(1);
                    if (m_state.items.Hash() == serverHash)
                        return true;

                    Log::Info("[Items] items.dat changed (hash %u), making the client download it", serverHash);
                    m_state.items.SetHash(serverHash);
                    call[1] = (uint32)0;

                    std::vector<uint8> data = call.Serialize();
                    Send(Client, game, data.data(), (uint32)data.size());
                    return false;
                }

                // Tracks players and gives the local player infinite zoom (mstate|1).
                case "OnSpawn"_hash: {
                    if (call.Size() < 2)
                        return true;

                    string spawnData = call.GetString(1);
                    TextPacket spawn(spawnData);

                    Player player;
                    player.netID = Strings::ToInt(spawn.Get("netID")).value_or(-1);
                    player.userID = Strings::ToInt(spawn.Get("userID")).value_or(-1);
                    player.name = Strings::StripColorCodes(spawn.Get("name"));
                    bool isLocal = spawn.Get("type") == "local";

                    if (player.netID != -1) {
                        if (isLocal) {
                            if (m_state.local.netID != -1 && m_state.local.netID != player.netID)
                                m_state.players.erase(m_state.local.netID);

                            m_state.local.netID = player.netID;
                            m_state.local.userID = player.userID;
                            m_state.local.name = player.name;
                        }
                        m_state.players[player.netID] = player;
                    }

                    if (!isLocal)
                        return true;

                    size_t pos = spawnData.find("mstate|");
                    if (pos == string::npos || pos + 7 >= spawnData.size())
                        return true;

                    spawnData[pos + 7] = '1';
                    call[1] = spawnData;

                    std::vector<uint8> data = call.Serialize();
                    Send(Client, game, data.data(), (uint32)data.size());
                    return false;
                }

                case "OnRemove"_hash: {
                    TextPacket remove(call.GetString(1));
                    int32 netID = Strings::ToInt(remove.Get("netID")).value_or(-1);
                    if (netID == -1)
                        return true;

                    // the local player leaving means the whole world is gone
                    if (netID == m_state.local.netID)
                        m_state.ClearPlayers();
                    else
                        m_state.players.erase(netID);
                    return true;
                }

                default:
                    return true;
            }
        }

        case PACKET_SEND_ITEM_DATABASE_DATA: {
            if (!extra)
                break;

            // value is the uncompressed size when the payload is zlib compressed
            std::vector<uint8> inflated;
            const uint8* data = extra;
            size_t size = game.extraDataSize;
            if (game.value != 0) {
                if (!Zlib::Inflate(extra, size, (size_t)game.value, inflated)) {
                    Log::Error("[Items] Could not decompress items.dat");
                    break;
                }
                data = inflated.data();
                size = inflated.size();
            }

            if (m_state.items.Load(data, size))
                Log::Info("[Items] Loaded %zu items (items.dat version %u)", m_state.items.Count(),
                          m_state.items.Version());
            break;
        }

        case PACKET_SEND_MAP_DATA: {
            m_state.ClearPlayers();
            if (extra && m_state.world.Load(extra, game.extraDataSize, m_state.items))
                Log::Info("[World] Entered %s", m_state.world.GetName().c_str());
            break;
        }

        case PACKET_STATE: {
            auto it = m_state.players.find(game.netID);
            if (it != m_state.players.end())
                it->second.tile = {(int32)(game.pos.x / 32), (int32)(game.pos.y / 32)};
            break;
        }

        case PACKET_TILE_CHANGE_REQUEST: {
            Tile* tile = m_state.world.GetTile(game.tileX, game.tileY);
            if (!tile)
                break;

            if (game.value == 18) {
                // a tile change to the fist (18) means the tile was destroyed: the foreground goes first, then the
                // background
                if (tile->foreground != 0) {
                    tile->foreground = 0;
                    tile->flags &= ~TILE_FLAG_HAS_EXTRA_DATA;
                    tile->extraType = TILE_EXTRA_NONE;
                } else {
                    tile->background = 0;
                }
                break;
            }

            const Item* item = m_state.items.Get(game.value);
            if (item && item->type == ITEM_TYPE_BACKGROUND)
                tile->background = (uint16)game.value;
            else
                tile->foreground = (uint16)game.value;
            break;
        }

        case PACKET_SEND_TILE_UPDATE_DATA: {
            if (extra)
                m_state.world.UpdateTile(game.tileX, game.tileY, extra, game.extraDataSize, m_state.items);
            break;
        }

        case PACKET_ITEM_CHANGE_OBJECT: {
            World& world = m_state.world;
            if (!world.IsLoaded())
                break;

            // netID encodes the operation: -3 modified, -1 added, anything else removed
            uint8 count = (uint8)game.floatValue;
            if (game.netID == -3)
                world.ModifyDroppedItem((uint32)game.targetID, (uint16)game.value, count, game.pos, game.byte1);
            else if (game.netID == -1)
                world.AddDroppedItem((uint16)game.value, count, game.pos, game.byte1);
            else
                world.RemoveDroppedItem((uint32)game.value);
            break;
        }

        case PACKET_SEND_INVENTORY_STATE:
        case PACKET_MODIFY_ITEM_INVENTORY: {
            if (extra) {
                if (!m_state.inventory.Load(extra, game.extraDataSize))
                    Log::Error("[Inventory] Could not read the inventory");
            } else if (game.type == PACKET_MODIFY_ITEM_INVENTORY && game.value != 0) {
                m_state.inventory.SetCount((uint16)game.value, game.byte1);
            }
            break;
        }
    }
    return true;
}

// Client -> server

bool Proxy::OnPacketFromClient(const PacketView& packet) {
    switch (packet.Type()) {
        case MESSAGE_GENERIC_TEXT:
        case MESSAGE_GAME_MESSAGE: {
            string_view text = packet.Text();
            if (text.find("action|quit") != string_view::npos) // also matches action|quit_to_exit
                m_state.LeaveWorld(false);

            TextPacket textPacket(text);
            string_view action = textPacket.Get("action");

            // proxy commands and the proxy's own dialogs never reach the server
            if (action == "input")
                return !m_commands.HandleChat(textPacket.Get("text"));
            if (action == "dialog_return")
                return !m_commands.HandleDialogReturn(textPacket);
            return true;
        }

        case MESSAGE_GAME_PACKET:
            break;

        default:
            return true;
    }

    GameUpdatePacket game;
    if (!packet.GetGamePacket(game))
        return true;

    switch (game.type) {
        // Holding left shift gives long punch; shift + left click walks to the clicked tile.
        case PACKET_STATE: {
            m_state.local.pos = game.pos;

            bool shiftDown = Platform::IsKeyDown(VK_LSHIFT);
            if (shiftDown != m_state.local.longPunchActive)
                Send(Client, m_state.local.SetLongPunch(shiftDown));

            if (shiftDown && Platform::IsKeyDown(VK_LBUTTON)) {
                WalkTo(*this, {game.tileX, game.tileY});
                return false;
            }
            return true;
        }

        // shift + punch/place belongs to long punch and pathfinding, not to the game
        case PACKET_TILE_CHANGE_REQUEST:
            return !Platform::IsKeyDown(VK_SHIFT);

        default:
            return true;
    }
}

// Connections

bool Proxy::Start(const char* host, uint16 port) {
    ENetAddress address{};
    address.port = port;
    if (enet_address_set_host(&address, host) != 0)
        return false;

    m_clientHost = enet_host_create(&address, MAX_CLIENTS, CHANNELS, 0, 0);
    if (!m_clientHost)
        return false;

    // CRC32 checksums and range-coder compression, like the game
    m_clientHost->checksum = enet_crc32;
    enet_host_compress_with_range_coder(m_clientHost);
    return true;
}

void Proxy::Stop() {
    CloseServerConnection();

    if (m_clientHost) {
        if (m_client)
            enet_peer_disconnect_now(m_client, 0);
        enet_host_destroy(m_clientHost);
        m_clientHost = nullptr;
        m_client = nullptr;
    }
}

void Proxy::Poll(uint32 timeoutMs) {
    if (!m_clientHost)
        return;

    ENetEvent event;
    if (enet_host_service(m_clientHost, &event, timeoutMs) > 0) {
        do {
            HandleClientEvent(event);
        } while (m_clientHost && enet_host_service(m_clientHost, &event, 0) > 0);
    }

    if (m_serverHost) {
        while (enet_host_service(m_serverHost, &event, 0) > 0)
            HandleServerEvent(event);
    }

    if (m_server && !m_serverConnected && Platform::NowMs() - m_connectStartedMs > Config::SERVER_CONNECT_TIMEOUT_MS) {
        Log::Error("[Proxy] Timed out connecting to the game server");
        CloseServerConnection();
        DisconnectClient();
    }

    RunDueTasks();
}

void Proxy::HandleClientEvent(ENetEvent& event) {
    switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT: {
            m_client = event.peer;
            enet_peer_timeout(m_client, 0, ENET_PEER_TIMEOUT_MAXIMUM / 2, 0);

            std::optional<Endpoint> target = m_redirecting ? m_redirectTarget : m_serverData.GetGameServer();
            m_redirecting = false;

            if (!target) {
                Log::Error("[Proxy] Client connected, but no game server is known yet (server_data.php not seen)");
                DisconnectClient();
                break;
            }

            Log::Info("[Proxy] Client connected, connecting to %s:%u", target->host.c_str(), target->port);
            ConnectToServer(*target);
            break;
        }

        case ENET_EVENT_TYPE_RECEIVE: {
            if (event.peer == m_client) {
                PacketView view{event.packet->data, event.packet->dataLength};
                if (OnPacketFromClient(view)) {
                    if (m_server && m_serverConnected)
                        SendPacket(Server, view.data, view.size, event.channelID, event.packet->flags);
                    else if (m_server)
                        m_pendingToServer.push_back({std::vector<uint8>(view.data, view.data + view.size),
                                                     event.channelID, event.packet->flags});
                }
            }
            enet_packet_destroy(event.packet);
            break;
        }

        case ENET_EVENT_TYPE_DISCONNECT: {
            if (event.peer != m_client)
                break; // an earlier client

            m_client = nullptr;
            if (m_redirecting) {
                Log::Info("[Proxy] Client is switching servers");
            } else {
                Log::Warn("[Proxy] Client disconnected");
                CloseServerConnection();
            }
            break;
        }

        default:
            break;
    }
}

void Proxy::HandleServerEvent(ENetEvent& event) {
    switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT: {
            m_serverConnected = true;
            Log::Info("[Proxy] Connected to the game server");

            for (const PendingPacket& pending : m_pendingToServer)
                SendPacket(Server, pending.data.data(), pending.data.size(), pending.channel, pending.flags);
            m_pendingToServer.clear();
            break;
        }

        case ENET_EVENT_TYPE_RECEIVE: {
            PacketView view{event.packet->data, event.packet->dataLength};
            if (OnPacketFromServer(view))
                SendPacket(Client, view.data, view.size, event.channelID, event.packet->flags);
            enet_packet_destroy(event.packet);
            break;
        }

        case ENET_EVENT_TYPE_DISCONNECT: {
            if (event.peer != m_server)
                break;

            if (m_serverConnected)
                Log::Warn("[Proxy] The game server closed the connection");
            else
                Log::Error("[Proxy] Could not connect to the game server");

            m_server = nullptr;
            m_serverConnected = false;
            m_pendingToServer.clear();

            if (!m_redirecting)
                DisconnectClient();
            break;
        }

        default:
            break;
    }
}

void Proxy::ConnectToServer(const Endpoint& server) {
    CloseServerConnection();

    ENetAddress address{};
    address.port = server.port;
    if (enet_address_set_host(&address, server.host.c_str()) != 0) {
        Log::Error("[Proxy] Cannot resolve %s", server.host.c_str());
        DisconnectClient();
        return;
    }

    m_serverHost = enet_host_create(nullptr, 1, CHANNELS, 0, 0);
    if (!m_serverHost) {
        DisconnectClient();
        return;
    }

    // CRC32 checksums and range-coder compression, like the game
    m_serverHost->checksum = enet_crc32;
    enet_host_compress_with_range_coder(m_serverHost);
    m_serverHost->useNewPacket = 1;

    m_server = enet_host_connect(m_serverHost, &address, CHANNELS, 0);
    m_serverConnected = false;
    m_connectStartedMs = Platform::NowMs();
    if (!m_server)
        DisconnectClient();
}

void Proxy::CloseServerConnection() {
    if (m_server)
        enet_peer_disconnect_now(m_server, 0);

    if (m_serverHost)
        enet_host_destroy(m_serverHost);

    m_serverHost = nullptr;
    m_server = nullptr;
    m_serverConnected = false;
    m_pendingToServer.clear();
}

void Proxy::DisconnectClient() {
    if (!m_client)
        return;

    enet_peer_disconnect(m_client, 0);
    enet_host_flush(m_clientHost);
    m_client = nullptr;
}

void Proxy::Schedule(uint32 delayMs, std::function<void()> task) {
    m_tasks.emplace(Platform::NowMs() + delayMs, std::move(task));
}

void Proxy::RedirectNextConnection(const Endpoint& server) {
    m_redirecting = true;
    m_redirectTarget = server;
}

void Proxy::RunDueTasks() {
    uint64 now = Platform::NowMs();
    while (!m_tasks.empty() && m_tasks.begin()->first <= now) {
        auto task = std::move(m_tasks.begin()->second);
        m_tasks.erase(m_tasks.begin());
        task();
    }
}
