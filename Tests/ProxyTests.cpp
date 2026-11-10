// Unit checks plus an end-to-end proxy test: a fake game client and fake game servers talk through the real
// Proxy over ENet on localhost, so the packet flow can be checked without the game.
//
// Usage: ProxyTests <items.dat> <world.bin>
#include "Core/Log.h"
#include "Features/Pathfinding/Pathfinding.h"
#include "Proxy/Proxy.h"
#include "Network/ServerData.h"
#include "Packets/TextPacket.h"
#include "Packets/VariantList.h"
#include "Utils/Strings.h"

#include <enet/enet.h>

#include <cstdio>
#include <fstream>
#include <functional>
#include <iterator>

static int s_failures = 0;

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            std::printf("  FAILED line %d: %s\n", __LINE__, #condition);                                               \
            ++s_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static std::vector<uint8> ReadFile(const char* path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<uint8>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

// Unit checks

static void TestVariantList() {
    std::printf("VariantList\n");
    VariantList call{"OnSendToServer", (int32)17091, (uint32)42, 1.5f, string("1.2.3.4|door|uuid"), Vec2f{3, 4}};
    std::vector<uint8> data = call.Serialize();

    VariantList parsed;
    CHECK(parsed.Parse(data.data(), data.size()));
    CHECK(parsed.Size() == 6);
    CHECK(parsed.GetString(0) == "OnSendToServer");
    CHECK(parsed.GetInt(1) == 17091);
    CHECK(parsed.GetUInt(2) == 42);
    CHECK(std::get<float>(parsed[3]) == 1.5f);
    CHECK(parsed.GetString(4) == "1.2.3.4|door|uuid");
    CHECK(std::get<Vec2f>(parsed[5]).y == 4.0f);

    CHECK(!parsed.Parse(data.data(), data.size() - 1)); // truncated
}

static void TestTextPacket() {
    std::printf("TextPacket\n");
    TextPacket packet("action|input\r\n|text|/warp START\nbuttonClicked|gscan_block\n");
    CHECK(packet.Get("action") == "input");
    CHECK(packet.Get("text") == "/warp START");
    CHECK(packet.Get("buttonClicked") == "gscan_block");
    CHECK(!packet.Has("missing"));
}

static void TestServerDataRedirect() {
    std::printf("RedirectServerData\n");
    string data = "server|52.1.2.3\nport|17198\ntype|1\n#maint|x\nmeta|abc";
    string out;
    Endpoint original;
    CHECK(RedirectServerData(data, {"127.0.0.1", 19000}, out, original));
    CHECK(original.host == "52.1.2.3" && original.port == 17198);
    CHECK(out == "server|127.0.0.1\nport|19000\ntype|1\n#maint|x\nmeta|abc");
    CHECK(!RedirectServerData("type|1", {"127.0.0.1", 19000}, out, original));
}

static void TestStrings() {
    std::printf("Strings\n");
    CHECK(Strings::ToInt(" 42 ") == 42);
    CHECK(!Strings::ToInt("4x"));
    CHECK(!Strings::ToFloat("x"));
    CHECK(Strings::StripColorCodes("`2Bob`` the `wGreat") == "Bob the Great");
}

static void TestPathfinding(const ItemDatabase& items, const World& world) {
    std::printf("Pathfinding\n");
    // the top rows of the test world are empty air
    std::vector<Vec2i> path = FindPath(world, items, {10, 2}, {20, 2}, 2);
    CHECK(path.size() == 11);
    CHECK(!path.empty() && path.front().x == 10 && path.back().x == 20);
    CHECK(FindPath(world, items, {10, 2}, {10, 2}, 2).size() == 1);
    CHECK(FindPath(world, items, {10, 2}, {500, 2}, 2).empty());
}

// Proxy end-to-end test

struct FakeHost {
    ENetHost* host = nullptr;
    ENetPeer* peer = nullptr; // the single connected peer
    bool connected = false;
    int connects = 0;
    std::vector<std::vector<uint8>> received;

    void Create(const char* ip, uint16 port) {
        ENetAddress address{};
        address.port = port;
        enet_address_set_host(&address, ip);
        host = enet_host_create(port ? &address : nullptr, 4, 2, 0, 0);
        host->checksum = enet_crc32;
        enet_host_compress_with_range_coder(host);
    }

    void Service() {
        ENetEvent event;
        while (enet_host_service(host, &event, 0) > 0) {
            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                peer = event.peer;
                connected = true;
                ++connects;
            } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                connected = false;
            } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                received.emplace_back(event.packet->data, event.packet->data + event.packet->dataLength);
                enet_packet_destroy(event.packet);
            }
        }
    }

    void Send(const std::vector<uint8>& data) {
        enet_peer_send(peer, 0, enet_packet_create(data.data(), data.size(), ENET_PACKET_FLAG_RELIABLE));
    }
};

// Finds a received call-function by name whose second argument contains `text` (if given).
static bool FindCall(const FakeHost& host, const char* name, VariantList& out, string_view text = {}) {
    for (const auto& data : host.received) {
        PacketView view{data.data(), data.size()};
        GameUpdatePacket packet;
        const uint8* extra = nullptr;
        if (view.Type() == MESSAGE_GAME_PACKET && view.GetGamePacket(packet, &extra) &&
            packet.type == PACKET_CALL_FUNCTION && extra && out.Parse(extra, packet.extraDataSize) &&
            out.GetString(0) == name && out.GetString(1).find(text) != string::npos)
            return true;
    }
    return false;
}

static bool ReceivedText(const FakeHost& host, string_view text) {
    for (const auto& data : host.received) {
        PacketView view{data.data(), data.size()};
        if (view.Text().find(text) != string_view::npos)
            return true;
    }
    return false;
}

static std::vector<uint8> MakeCall(const VariantList& call) {
    GameUpdatePacket packet;
    packet.type = PACKET_CALL_FUNCTION;
    packet.netID = -1;
    std::vector<uint8> data = call.Serialize();
    return MakeGamePacket(packet, data.data(), (uint32)data.size());
}

static void TestProxy(const std::vector<uint8>& itemData, const std::vector<uint8>& worldData) {
    std::printf("Proxy (end to end)\n");

    FakeHost server1, server2, client;
    server1.Create("127.0.0.1", 17999);
    server2.Create("127.0.0.1", 17998);
    client.Create("127.0.0.1", 0);

    ServerData serverData;
    Proxy proxy(serverData);
    CHECK(proxy.Start("127.0.0.1", 17000));
    proxy.State().items.Load(itemData.data(), itemData.size());

    auto pump = [&](std::function<bool()> done, int maxMs = 3000) {
        for (int i = 0; i < maxMs && !done(); ++i) {
            proxy.Poll(1);
            server1.Service();
            server2.Service();
            client.Service();
        }
        return done();
    };

    // 1. without server_data.php the proxy needs a target; use the redirect path for the first connection
    proxy.RedirectNextConnection({"127.0.0.1", 17999});
    ENetAddress proxyAddress{};
    enet_address_set_host(&proxyAddress, "127.0.0.1");
    proxyAddress.port = 17000;
    client.peer = enet_host_connect(client.host, &proxyAddress, 2, 0);
    CHECK(pump([&] { return client.connected && server1.connected; }));

    // 2. normal chat is forwarded
    client.Send(MakeTextPacket(MESSAGE_GENERIC_TEXT, "action|input\n|text|hello"));
    CHECK(pump([&] { return ReceivedText(server1, "hello"); }));

    // 3. a proxy command is answered by the proxy and never reaches the server
    client.Send(MakeTextPacket(MESSAGE_GENERIC_TEXT, "action|input\n|text|/gs"));
    VariantList call;
    CHECK(pump([&] { return FindCall(client, "OnDialogRequest", call); }));
    CHECK(call.GetString(1).find("gscan_block") != string::npos);
    CHECK(!ReceivedText(server1, "/gs"));

    // 4. growscan outside a world
    client.Send(MakeTextPacket(MESSAGE_GENERIC_TEXT,
                               "action|dialog_return\ndialog_name|growscan_dialog\nbuttonClicked|gscan_block\n"));
    CHECK(pump([&] { return FindCall(client, "OnConsoleMessage", call, "must be in a world"); }));

    // 5. OnSpawn: the local player is tracked and gets mstate|1
    server1.Send(
        MakeCall({"OnSpawn", string("spawn|avatar\nnetID|5\nuserID|7\nname|`2Bob``\ntype|local\nmstate|0\n")}));
    CHECK(pump([&] { return FindCall(client, "OnSpawn", call); }));
    CHECK(call.GetString(1).find("mstate|1") != string::npos);
    CHECK(proxy.State().local.netID == 5 && proxy.State().local.name == "Bob");

    // 6. map data is parsed; growscan now lists the blocks
    GameUpdatePacket map;
    map.type = PACKET_SEND_MAP_DATA;
    server1.Send(MakeGamePacket(map, worldData.data(), (uint32)worldData.size()));
    CHECK(pump([&] { return proxy.State().world.IsLoaded(); }));
    client.received.clear();
    client.Send(MakeTextPacket(MESSAGE_GENERIC_TEXT,
                               "action|dialog_return\ndialog_name|growscan_dialog\nbuttonClicked|gscan_block\n"));
    CHECK(pump([&] { return FindCall(client, "OnDialogRequest", call); }));
    CHECK(call.GetString(1).find("World blocks") != string::npos && call.GetString(1).find("Bedrock") != string::npos);

    // 7. antigravity: one packet now, the rest a second later from a scheduled task
    client.received.clear();
    client.Send(MakeTextPacket(MESSAGE_GENERIC_TEXT, "action|input\n|text|/ag"));
    CHECK(pump([&] { return FindCall(client, "OnConsoleMessage", call, "Activated!"); }, 2500));

    // 8. OnSendToServer: the client is pointed back at the proxy, and on reconnect the proxy uses the new server
    server1.Send(MakeCall({"OnSendToServer", (int32)17998, (int32)1, (int32)2, string("127.0.0.1|0|abc"), (int32)1}));
    CHECK(pump([&] { return FindCall(client, "OnSendToServer", call); }));
    CHECK(call.GetInt(1) == 19000 && call.GetString(4) == "127.0.0.1|0|abc");

    enet_peer_disconnect(client.peer, 0);
    CHECK(pump([&] { return !client.connected; }));
    client.peer = enet_host_connect(client.host, &proxyAddress, 2, 0);
    CHECK(pump([&] { return client.connected && server2.connected; }));

    // 9. the server closing the connection disconnects the client
    enet_peer_disconnect(server2.peer, 0);
    CHECK(pump([&] { return !client.connected; }));

    proxy.Stop();
    enet_host_destroy(client.host);
    enet_host_destroy(server1.host);
    enet_host_destroy(server2.host);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::printf("usage: ProxyTests <items.dat> <world.bin>\n");
        return 1;
    }

    Log::Init();
    enet_initialize();

    std::vector<uint8> itemData = ReadFile(argv[1]);
    std::vector<uint8> worldData = ReadFile(argv[2]);
    ItemDatabase items;
    World world;
    CHECK(items.Load(itemData.data(), itemData.size()));
    CHECK(world.Load(worldData.data(), worldData.size(), items));

    TestVariantList();
    TestTextPacket();
    TestServerDataRedirect();
    TestStrings();
    TestPathfinding(items, world);
    TestProxy(itemData, worldData);

    enet_deinitialize();
    std::printf(s_failures == 0 ? "\nALL PASSED\n" : "\n%d CHECK(S) FAILED\n", s_failures);
    return s_failures == 0 ? 0 : 1;
}
