#pragma once

#include "Core/Types.h"

#include <vector>

// Item types (items.dat "type" byte) that the proxy cares about.
enum ItemType : uint8 {
    ITEM_TYPE_USER_DOOR = 2,
    ITEM_TYPE_LOCK = 3,
    ITEM_TYPE_SIGN = 10,
    ITEM_TYPE_DOOR = 13,
    ITEM_TYPE_BACKGROUND = 18,
    ITEM_TYPE_SEED = 19,
    ITEM_TYPE_PORTAL = 26,
    ITEM_TYPE_RACE_FLAG = 30,
    ITEM_TYPE_MAILBOX = 33,
    ITEM_TYPE_BULLETIN = 34,
    ITEM_TYPE_COMPONENT = 36,
    ITEM_TYPE_DICE = 37,
    ITEM_TYPE_PROVIDER = 38,
    ITEM_TYPE_ACHIEVEMENT = 40,
    ITEM_TYPE_SUNGATE = 43,
    ITEM_TYPE_HEART_MONITOR = 46,
    ITEM_TYPE_DONATION_BOX = 47,
    ITEM_TYPE_TOYBOX = 48,
    ITEM_TYPE_MANNEQUIN = 49,
    ITEM_TYPE_MAGIC_EGG = 51,
    ITEM_TYPE_TEAM = 52,
    ITEM_TYPE_GAME_GENERATOR = 53,
    ITEM_TYPE_XENONITE = 54,
    ITEM_TYPE_DRESSUP = 55,
    ITEM_TYPE_CRYSTAL = 56,
    ITEM_TYPE_BURGLAR = 57,
    ITEM_TYPE_SPOTLIGHT = 59,
    ITEM_TYPE_DISPLAY_BLOCK = 61,
    ITEM_TYPE_VENDING = 62,
    ITEM_TYPE_FISH_TANK = 63,
    ITEM_TYPE_SOLAR = 65,
    ITEM_TYPE_FORGE = 66,
    ITEM_TYPE_GIVING_TREE = 67,
    ITEM_TYPE_GIVING_TREE_STUMP = 68,
    ITEM_TYPE_STEAMPUNK = 69,
    ITEM_TYPE_STEAM_ORGAN = 71,
    ITEM_TYPE_TAMAGOTCHI = 72,
    ITEM_TYPE_SEWING = 73,
    ITEM_TYPE_FLAG = 74,
    ITEM_TYPE_LOBSTER_TRAP = 75,
    ITEM_TYPE_ART_CANVAS = 76,
    ITEM_TYPE_BATTLE_CAGE = 77,
    ITEM_TYPE_PET_TRAINER = 78,
    ITEM_TYPE_STEAM_ENGINE = 79,
    ITEM_TYPE_LOCK_BOT = 80,
    ITEM_TYPE_WEATHER_SPECIAL = 81,
    ITEM_TYPE_SPIRIT_STORAGE = 82,
    ITEM_TYPE_DISPLAY_SHELF = 83,
    ITEM_TYPE_VIP_DOOR = 84,
    ITEM_TYPE_CHALLENGE_TIMER = 85,
    ITEM_TYPE_CHALLENGE_FLAG = 86,
    ITEM_TYPE_FISH_MOUNT = 87,
    ITEM_TYPE_PORTRAIT = 88,
    ITEM_TYPE_WEATHER_SPECIAL2 = 89,
    ITEM_TYPE_FOSSIL_PREP = 91,
    ITEM_TYPE_DNA_MACHINE = 92,
    ITEM_TYPE_BLASTER = 93,
    ITEM_TYPE_CHEM_TANK = 96,
    ITEM_TYPE_STORAGE = 97,
    ITEM_TYPE_OVEN = 98,
    ITEM_TYPE_SUPER_MUSIC = 99,
    ITEM_TYPE_GEIGER_CHARGER = 100,
    ITEM_TYPE_ADVENTURE_RESET = 101,
    ITEM_TYPE_TOMB_ROBBER = 102,
    ITEM_TYPE_FACTION = 103,
    ITEM_TYPE_FISHING_BLOCK = 110,
    ITEM_TYPE_SUCKER = 111,
    ITEM_TYPE_ROBOT = 113,
    ITEM_TYPE_LUCKY_TICKET = 115,
    ITEM_TYPE_STATS_BLOCK = 116,
    ITEM_TYPE_FIELD_NODE = 117,
    ITEM_TYPE_OUIJA_BOARD = 118,
    ITEM_TYPE_AUTO_BREAK = 123,
    ITEM_TYPE_AUTO_HARVEST = 124,
    ITEM_TYPE_AUTO_HARVEST_SUCKER = 125,
    ITEM_TYPE_LIGHTNING_CLOUD = 126,
    ITEM_TYPE_PHASED_BLOCK = 127,
    ITEM_TYPE_PASSWORD_STORAGE = 130,
    ITEM_TYPE_PHASED_BLOCK2 = 131,
    ITEM_TYPE_PVE_NPC = 133,
    ITEM_TYPE_INFINITY_WEATHER = 134,
    ITEM_TYPE_COMPLETIONIST = 137,
    ITEM_TYPE_FEEDING_BLOCK = 140,
    ITEM_TYPE_KRANKENS_BLOCK = 141,
    ITEM_TYPE_FRIENDS_ENTRANCE = 142,
};

enum CollisionType : uint8 {
    COLLISION_NONE = 0,
    COLLISION_SOLID = 1,
    COLLISION_JUMP_THROUGH = 2, // solid from above
    COLLISION_GATEWAY = 3,      // passable for players with access
    COLLISION_IF_OFF = 4,       // solid unless switched on
    COLLISION_ONE_WAY = 5,
    COLLISION_VIP = 6,
    COLLISION_JUMP_DOWN = 7, // solid from below
};

struct Item {
    int32 id = 0;
    uint16 flags = 0;
    uint8 type = 0;
    uint8 material = 0;
    string name;
    string texture;
    uint8 textureX = 0;
    uint8 textureY = 0;
    uint8 collisionType = COLLISION_NONE;
    uint8 breakHits = 0;
    int16 rarity = 0;
    uint8 maxAmount = 200;
    uint32 growTime = 0;
    string description;
};

// Item definitions from the server's items.dat.
class ItemDatabase {
public:
    // Loads an uncompressed items.dat. Returns false (and keeps the previous data) on failure.
    bool Load(const uint8* data, size_t size);

    const Item* Get(int32 id) const;
    size_t Count() const { return m_items.size(); }
    uint16 Version() const { return m_version; }

    // Hash of the server's items.dat that our data matches; 0 if we never received one.
    uint32 Hash() const { return m_hash; }
    void SetHash(uint32 hash) { m_hash = hash; }

    // Display name, or "Item <id>" if unknown.
    string GetName(int32 id) const;

private:
    std::vector<Item> m_items;
    uint16 m_version = 0;
    uint32 m_hash = 0;
};
