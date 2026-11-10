#pragma once

#include "Core/Types.h"

#include <vector>

// Every ENet packet starts with a uint32 message type.
enum MessageType : uint32 {
    MESSAGE_UNKNOWN = 0,
    MESSAGE_SERVER_HELLO = 1,
    MESSAGE_GENERIC_TEXT = 2,
    MESSAGE_GAME_MESSAGE = 3,
    MESSAGE_GAME_PACKET = 4,
    MESSAGE_ERROR = 5,
    MESSAGE_TRACK = 6,
    MESSAGE_CLIENT_LOG_REQUEST = 7,
    MESSAGE_CLIENT_LOG_RESPONSE = 8,
};

// GameUpdatePacket::type
enum GamePacketType : uint8 {
    PACKET_STATE = 0,
    PACKET_CALL_FUNCTION = 1,
    PACKET_UPDATE_STATUS = 2,
    PACKET_TILE_CHANGE_REQUEST = 3,
    PACKET_SEND_MAP_DATA = 4,
    PACKET_SEND_TILE_UPDATE_DATA = 5,
    PACKET_SEND_TILE_UPDATE_DATA_MULTIPLE = 6,
    PACKET_TILE_ACTIVATE_REQUEST = 7,
    PACKET_TILE_APPLY_DAMAGE = 8,
    PACKET_SEND_INVENTORY_STATE = 9,
    PACKET_ITEM_ACTIVATE_REQUEST = 10,
    PACKET_ITEM_ACTIVATE_OBJECT_REQUEST = 11,
    PACKET_SEND_TILE_TREE_STATE = 12,
    PACKET_MODIFY_ITEM_INVENTORY = 13,
    PACKET_ITEM_CHANGE_OBJECT = 14,
    PACKET_SEND_LOCK = 15,
    PACKET_SEND_ITEM_DATABASE_DATA = 16,
    PACKET_SEND_PARTICLE_EFFECT = 17,
    PACKET_SET_ICON_STATE = 18,
    PACKET_ITEM_EFFECT = 19,
    PACKET_SET_CHARACTER_STATE = 20,
    PACKET_PING_REPLY = 21,
    PACKET_PING_REQUEST = 22,
    PACKET_GOT_PUNCHED = 23,
    PACKET_APP_CHECK_RESPONSE = 24,
    PACKET_APP_INTEGRITY_FAIL = 25,
    PACKET_DISCONNECT = 26,
    PACKET_BATTLE_JOIN = 27,
    PACKET_BATTLE_EVENT = 28,
    PACKET_USE_DOOR = 29,
    PACKET_SEND_PARENTAL = 30,
    PACKET_GONE_FISHIN = 31,
    PACKET_STEAM = 32,
    PACKET_PET_BATTLE = 33,
    PACKET_NPC = 34,
    PACKET_SPECIAL = 35,
    PACKET_SEND_PARTICLE_EFFECT_V2 = 36,
    PACKET_ACTIVE_ARROW_TO_ITEM = 37,
    PACKET_SELECT_TILE_INDEX = 38,
    PACKET_SEND_PLAYER_TRIBUTE_DATA = 39,
    PACKET_ON_STEP_TILE_MOD = 40,
};

enum GamePacketFlag : uint32 {
    PACKET_FLAG_EXTENDED_DATA = 1 << 3, // extraDataSize bytes follow the packet
};

// Binary game packet (message type 4). The meaning of the generic fields depends on the packet type.
#pragma pack(push, 1)
struct GameUpdatePacket {
    uint8 type = PACKET_STATE;
    uint8 byte1 = 0;    // object flags, inventory count, ...
    uint8 byte2 = 0;    // punch range
    uint8 byte3 = 0;    // build range
    int32 netID = 0;    // source player
    int32 targetID = 0; // other player or object
    uint32 flags = 0;
    float floatValue = 0; // dropped item count, ...
    int32 value = 0;      // item id, uncompressed size, call delay, ...
    Vec2f pos;            // pixels
    Vec2f speed;
    float rotation = 0;
    int32 tileX = 0;
    int32 tileY = 0;
    uint32 extraDataSize = 0;
};
#pragma pack(pop)

static_assert(sizeof(GameUpdatePacket) == 56, "GameUpdatePacket must match the wire format");

// A received ENet packet: message type plus payload.
struct PacketView {
    const uint8* data = nullptr;
    size_t size = 0;

    MessageType Type() const;

    // Text of a text message, without the trailing NUL.
    string_view Text() const;

    // Game packet and its extra data. Fails if the packet is too short.
    bool GetGamePacket(GameUpdatePacket& out, const uint8** extraData = nullptr) const;
};

std::vector<uint8> MakeTextPacket(MessageType type, string_view text);
std::vector<uint8> MakeGamePacket(GameUpdatePacket packet, const void* extraData = nullptr, uint32 extraSize = 0);
