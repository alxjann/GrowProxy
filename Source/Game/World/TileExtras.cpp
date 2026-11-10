#include "Game/World/TileExtras.h"

#include "Game/Items/Items.h"

namespace {

    // Items whose tile extra layout differs from the rest of their type.
    constexpr uint16 ITEM_DATA_BEDROCK = 3760;
    constexpr uint16 ITEM_GUILD_LOCK = 5814;
    constexpr uint16 ITEM_PROVIDER_WITH_DATA = 5318;
    constexpr uint16 ITEM_PROVIDER_WITH_DATA_V17 = 10656;
    constexpr uint16 ITEM_FLAG_WITH_COUNTRY = 3394;
    constexpr uint16 ITEM_PORTRAIT_WITH_TEXT = 12958;

    // Clothing items that carry 8 extra bytes when worn by a mannequin or portrait.
    bool HasExtraClothData(uint16 itemID) {
        return itemID == 5712 || itemID == 10044;
    }

    // uint32 count followed by count elements of elementSize bytes.
    void SkipList(ByteReader& r, size_t elementSize) {
        r.Skip(r.Read<uint32>() * elementSize);
    }

}

uint8 GetTileExtraTypeForItem(int32 itemID, uint8 itemType) {
    if (itemID == ITEM_DATA_BEDROCK)
        return TILE_EXTRA_BEDROCK;

    switch (itemType) {
        case ITEM_TYPE_USER_DOOR:
        case ITEM_TYPE_DOOR:
        case ITEM_TYPE_PORTAL:
        case ITEM_TYPE_SUNGATE:
            return TILE_EXTRA_DOOR;
        case ITEM_TYPE_SIGN:
            return TILE_EXTRA_SIGN;
        case ITEM_TYPE_LOCK:
            return TILE_EXTRA_LOCK;
        case ITEM_TYPE_SEED:
            return TILE_EXTRA_SEED;
        case ITEM_TYPE_RACE_FLAG:
            return TILE_EXTRA_RACE_FLAG;
        case ITEM_TYPE_MAILBOX:
            return TILE_EXTRA_MAILBOX;
        case ITEM_TYPE_BULLETIN:
            return TILE_EXTRA_BULLETIN;
        case ITEM_TYPE_DICE:
        case ITEM_TYPE_COMPONENT:
            return TILE_EXTRA_COMPONENT;
        case ITEM_TYPE_PROVIDER:
            return TILE_EXTRA_PROVIDER;
        case ITEM_TYPE_ACHIEVEMENT:
            return TILE_EXTRA_ACHIEVEMENT;
        case ITEM_TYPE_HEART_MONITOR:
            return TILE_EXTRA_HEART_MONITOR;
        case ITEM_TYPE_DONATION_BOX:
            return TILE_EXTRA_DONATION_BOX;
        case ITEM_TYPE_TOYBOX:
            return TILE_EXTRA_TOYBOX;
        case ITEM_TYPE_MANNEQUIN:
            return TILE_EXTRA_MANNEQUIN;
        case ITEM_TYPE_MAGIC_EGG:
            return TILE_EXTRA_MAGIC_EGG;
        case ITEM_TYPE_TEAM:
            return TILE_EXTRA_TEAM;
        case ITEM_TYPE_GAME_GENERATOR:
            return TILE_EXTRA_GAME_GENERATOR;
        case ITEM_TYPE_XENONITE:
            return TILE_EXTRA_XENONITE;
        case ITEM_TYPE_DRESSUP:
            return TILE_EXTRA_DRESSUP;
        case ITEM_TYPE_CRYSTAL:
            return TILE_EXTRA_CRYSTAL;
        case ITEM_TYPE_BURGLAR:
            return TILE_EXTRA_BURGLAR;
        case ITEM_TYPE_SPOTLIGHT:
            return TILE_EXTRA_SPOTLIGHT;
        case ITEM_TYPE_DISPLAY_BLOCK:
            return TILE_EXTRA_DISPLAY_BLOCK;
        case ITEM_TYPE_VENDING:
            return TILE_EXTRA_VENDING;
        case ITEM_TYPE_FISH_TANK:
            return TILE_EXTRA_FISH_TANK;
        case ITEM_TYPE_SOLAR:
            return TILE_EXTRA_SOLAR;
        case ITEM_TYPE_FORGE:
            return TILE_EXTRA_FORGE;
        case ITEM_TYPE_GIVING_TREE:
            return TILE_EXTRA_GIVING_TREE;
        case ITEM_TYPE_GIVING_TREE_STUMP:
            return TILE_EXTRA_GIVING_TREE_STUMP;
        case ITEM_TYPE_STEAMPUNK:
        case ITEM_TYPE_STEAM_ORGAN:
            return TILE_EXTRA_STEAM_ORGAN;
        case ITEM_TYPE_TAMAGOTCHI:
            return TILE_EXTRA_TAMAGOTCHI;
        case ITEM_TYPE_SEWING:
            return TILE_EXTRA_SEWING;
        case ITEM_TYPE_FLAG:
            return TILE_EXTRA_FLAG;
        case ITEM_TYPE_LOBSTER_TRAP:
            return TILE_EXTRA_LOBSTER_TRAP;
        case ITEM_TYPE_ART_CANVAS:
            return TILE_EXTRA_ART_CANVAS;
        case ITEM_TYPE_BATTLE_CAGE:
            return TILE_EXTRA_BATTLE_CAGE;
        case ITEM_TYPE_PET_TRAINER:
            return TILE_EXTRA_PET_TRAINER;
        case ITEM_TYPE_STEAM_ENGINE:
            return TILE_EXTRA_STEAM_ENGINE;
        case ITEM_TYPE_LOCK_BOT:
            return TILE_EXTRA_LOCK_BOT;
        case ITEM_TYPE_WEATHER_SPECIAL:
            return TILE_EXTRA_WEATHER_SPECIAL;
        case ITEM_TYPE_SPIRIT_STORAGE:
            return TILE_EXTRA_SPIRIT_STORAGE;
        case ITEM_TYPE_DISPLAY_SHELF:
            return TILE_EXTRA_DISPLAY_SHELF;
        case ITEM_TYPE_VIP_DOOR:
            return TILE_EXTRA_VIP_DOOR;
        case ITEM_TYPE_CHALLENGE_TIMER:
            return TILE_EXTRA_CHALLENGE_TIMER;
        case ITEM_TYPE_CHALLENGE_FLAG:
            return TILE_EXTRA_CHALLENGE_FLAG;
        case ITEM_TYPE_FISH_MOUNT:
            return TILE_EXTRA_FISH_MOUNT;
        case ITEM_TYPE_PORTRAIT:
            return TILE_EXTRA_PORTRAIT;
        case ITEM_TYPE_WEATHER_SPECIAL2:
            return TILE_EXTRA_WEATHER_SPECIAL2;
        case ITEM_TYPE_FOSSIL_PREP:
            return TILE_EXTRA_FOSSIL_PREP;
        case ITEM_TYPE_DNA_MACHINE:
            return TILE_EXTRA_DNA_MACHINE;
        case ITEM_TYPE_BLASTER:
            return TILE_EXTRA_BLASTER;
        case ITEM_TYPE_CHEM_TANK:
            return TILE_EXTRA_CHEM_TANK;
        case ITEM_TYPE_STORAGE:
            return TILE_EXTRA_STORAGE;
        case ITEM_TYPE_OVEN:
            return TILE_EXTRA_OVEN;
        case ITEM_TYPE_SUPER_MUSIC:
            return TILE_EXTRA_SUPER_MUSIC;
        case ITEM_TYPE_GEIGER_CHARGER:
            return TILE_EXTRA_GEIGER_CHARGER;
        case ITEM_TYPE_ADVENTURE_RESET:
            return TILE_EXTRA_ADVENTURE_RESET;
        case ITEM_TYPE_TOMB_ROBBER:
            return TILE_EXTRA_TOMB_ROBBER;
        case ITEM_TYPE_FACTION:
            return TILE_EXTRA_FACTION;
        case ITEM_TYPE_FISHING_BLOCK:
            return TILE_EXTRA_TRAINING_FISH_PORT;
        case ITEM_TYPE_SUCKER:
            return TILE_EXTRA_SUCKER;
        case ITEM_TYPE_ROBOT:
            return TILE_EXTRA_ROBOT;
        case ITEM_TYPE_LUCKY_TICKET:
            return TILE_EXTRA_TICKET;
        case ITEM_TYPE_STATS_BLOCK:
            return TILE_EXTRA_STATS_BLOCK;
        case ITEM_TYPE_FIELD_NODE:
            return TILE_EXTRA_FIELD_NODE;
        case ITEM_TYPE_OUIJA_BOARD:
            return TILE_EXTRA_OUIJA_BOARD;
        case ITEM_TYPE_AUTO_BREAK:
            return TILE_EXTRA_AUTO_BREAK;
        case ITEM_TYPE_AUTO_HARVEST:
            return TILE_EXTRA_AUTO_HARVEST;
        case ITEM_TYPE_AUTO_HARVEST_SUCKER:
            return TILE_EXTRA_AUTO_HARVEST_SUCKER;
        case ITEM_TYPE_LIGHTNING_CLOUD:
            return TILE_EXTRA_LIGHTNING_IF_ON;
        case ITEM_TYPE_PHASED_BLOCK:
            return TILE_EXTRA_PHASED_BLOCK;
        case ITEM_TYPE_PASSWORD_STORAGE:
            return TILE_EXTRA_SAFE_VAULT;
        case ITEM_TYPE_PHASED_BLOCK2:
            return TILE_EXTRA_PHASED_BLOCK2;
        case ITEM_TYPE_PVE_NPC:
            return TILE_EXTRA_PVE_NPC;
        case ITEM_TYPE_INFINITY_WEATHER:
            return TILE_EXTRA_INFINITY_WEATHER;
        case ITEM_TYPE_COMPLETIONIST:
            return TILE_EXTRA_COMPLETIONIST;
        case ITEM_TYPE_FEEDING_BLOCK:
            return TILE_EXTRA_FEEDING_BLOCK;
        case ITEM_TYPE_KRANKENS_BLOCK:
            return TILE_EXTRA_KRANKENS_BLOCK;
        case ITEM_TYPE_FRIENDS_ENTRANCE:
            return TILE_EXTRA_FRIENDS_ENTRANCE;
        default:
            return TILE_EXTRA_NONE;
    }
}

void SkipTileExtra(ByteReader& r, uint8 type, uint16 fg, uint16 worldVersion) {
    r.Skip(1); // type byte

    switch (type) {
        case TILE_EXTRA_DOOR:
            r.SkipString(); // label
            r.Skip(1);      // flags
            break;

        case TILE_EXTRA_SIGN:
            r.SkipString(); // text
            r.Skip(4);
            break;

        case TILE_EXTRA_LOCK:
            r.Skip(1 + 4);  // flags, owner user id
            SkipList(r, 4); // access list
            if (worldVersion > 11)
                r.Skip(4); // minimum entry level
            if (worldVersion > 12)
                r.Skip(4); // world timer
            if (fg == ITEM_GUILD_LOCK)
                r.Skip(16);
            break;

        case TILE_EXTRA_SEED:
            r.Skip(4 + 1); // grow time, fruit count
            break;

        case TILE_EXTRA_MAILBOX:
        case TILE_EXTRA_BULLETIN:
        case TILE_EXTRA_DONATION_BOX:
            r.SkipString();
            r.SkipString();
            r.SkipString();
            r.Skip(1);
            break;

        case TILE_EXTRA_COMPONENT:
        case TILE_EXTRA_TEAM:
        case TILE_EXTRA_STATS_BLOCK:
            r.Skip(1);
            break;

        case TILE_EXTRA_PROVIDER:
            r.Skip(4); // grow time
            if (fg == ITEM_PROVIDER_WITH_DATA || (fg == ITEM_PROVIDER_WITH_DATA_V17 && worldVersion >= 17))
                r.Skip(4);
            break;

        case TILE_EXTRA_ACHIEVEMENT:
            r.Skip(4 + 1); // owner, achievement id
            break;

        case TILE_EXTRA_HEART_MONITOR:
            r.Skip(4);      // owner
            r.SkipString(); // player name
            break;

        case TILE_EXTRA_MANNEQUIN: {
            r.SkipString(); // text
            r.Skip(1 + 4);  // flags, hair color
            uint16 clothes[8] = {};
            r.ReadBytes(clothes, sizeof(clothes));
            if (HasExtraClothData(clothes[7]) || HasExtraClothData(clothes[3]))
                r.Skip(8);
            r.Skip(2); // ninth cloth slot
            break;
        }

        case TILE_EXTRA_MAGIC_EGG:
        case TILE_EXTRA_FORGE:
        case TILE_EXTRA_LOCK_BOT:
        case TILE_EXTRA_WEATHER_SPECIAL:
        case TILE_EXTRA_DISPLAY_BLOCK:
        case TILE_EXTRA_GEIGER_CHARGER:
        case TILE_EXTRA_PHASED_BLOCK:
        case TILE_EXTRA_FEEDING_BLOCK:
            r.Skip(4);
            break;

        case TILE_EXTRA_XENONITE:
            r.Skip(1 + 4);
            break;

        case TILE_EXTRA_DRESSUP:
            r.Skip(9 * 2); // clothes
            break;

        case TILE_EXTRA_CRYSTAL:
        case TILE_EXTRA_STORAGE:
            r.SkipString();
            break;

        case TILE_EXTRA_BURGLAR:
            r.SkipString();
            r.Skip(4 + 1);
            break;

        case TILE_EXTRA_VENDING:
            r.Skip(4 + 4); // item id, price
            break;

        case TILE_EXTRA_FISH_TANK:
        case TILE_EXTRA_SOLAR:
            r.Skip(1);
            SkipList(r, 4);
            break;

        case TILE_EXTRA_GIVING_TREE:
        case TILE_EXTRA_GIVING_TREE_STUMP:
            r.Skip(1 + 4 + 1);
            break;

        case TILE_EXTRA_STEAM_ORGAN:
            r.Skip(5);
            break;

        case TILE_EXTRA_TAMAGOTCHI:
            r.Skip(1);
            r.SkipString(); // name
            r.Skip(29);
            break;

        case TILE_EXTRA_SEWING:
            SkipList(r, 4); // bolts
            break;

        case TILE_EXTRA_FLAG:
            if (fg == ITEM_FLAG_WITH_COUNTRY)
                r.SkipString();
            break;

        case TILE_EXTRA_ART_CANVAS:
            r.Skip(4);
            r.SkipString();
            break;

        case TILE_EXTRA_BATTLE_CAGE:
            r.SkipString();    // cage name
            r.Skip(4 + 4 + 4); // pets
            break;

        case TILE_EXTRA_PET_TRAINER: {
            r.SkipString(); // trainer name
            uint32 petCount = r.Read<uint32>();
            r.Skip(4);
            r.Skip(petCount * 4);
            if (worldVersion > 23)
                r.SkipString();
            break;
        }

        case TILE_EXTRA_BEDROCK:
            r.Skip(21);
            break;

        case TILE_EXTRA_DISPLAY_SHELF:
        case TILE_EXTRA_AUTO_BREAK:
            r.Skip(16);
            break;

        case TILE_EXTRA_VIP_DOOR:
            r.Skip(1 + 4);  // flags, owner
            SkipList(r, 4); // access list
            break;

        case TILE_EXTRA_FISH_MOUNT:
            r.SkipString();
            r.Skip(5);
            break;

        case TILE_EXTRA_PORTRAIT: {
            r.SkipString(); // label
            r.Skip(16);
            uint16 items[3] = {};
            r.ReadBytes(items, sizeof(items));
            if (HasExtraClothData(items[0]) || HasExtraClothData(items[1]) || HasExtraClothData(items[2]))
                r.Skip(8);
            r.Skip(4);
            if (items[1] == ITEM_PORTRAIT_WITH_TEXT)
                r.SkipString();
            break;
        }

        case TILE_EXTRA_WEATHER_SPECIAL2:
            r.Skip(9);
            break;

        case TILE_EXTRA_OVEN:
            SkipList(r, 4);
            r.Skip(16);
            break;

        case TILE_EXTRA_SUPER_MUSIC:
            r.SkipString();
            r.Skip(4);
            break;

        case TILE_EXTRA_FACTION:
            r.Skip(4 + 1);
            break;

        case TILE_EXTRA_TRAINING_FISH_PORT:
            r.Skip(35);
            break;

        case TILE_EXTRA_SUCKER:
            r.Skip(4 + 4 + 1 + 1 + 4); // item id, count, sucking, planting, unknown
            break;

        case TILE_EXTRA_ROBOT:
            SkipList(r, 15);
            r.Skip(8);
            break;

        case TILE_EXTRA_GUILD_ITEM:
            r.Skip(17);
            break;

        case TILE_EXTRA_FIELD_NODE:
            r.Skip(4);      // expire time
            SkipList(r, 4); // nodes
            break;

        case TILE_EXTRA_OUIJA_BOARD:
            r.Skip(4);      // player count
            r.SkipString(); // type
            r.SkipString(); // command
            SkipList(r, 4); // items
            break;

        case TILE_EXTRA_AUTO_HARVEST_SUCKER:
            r.Skip(44);
            break;

        case TILE_EXTRA_LIGHTNING_IF_ON:
            r.Skip(12);
            break;

        case TILE_EXTRA_PHASED_BLOCK2: {
            uint32 state = r.Read<uint32>();
            r.Skip(2);
            if (state == 2)
                r.Skip(1); // ascii code
            break;
        }

        case TILE_EXTRA_INFINITY_WEATHER:
            r.Skip(4);
            SkipList(r, 4); // weathers
            break;

        case TILE_EXTRA_KRANKENS_BLOCK:
            r.Skip(1 + 4 + 3); // pattern, unknown, rgb
            break;

        case TILE_EXTRA_FRIENDS_ENTRANCE: {
            r.Skip(4 + 2); // owner, unknown
            uint16 allowed = r.Read<uint16>();
            r.Skip(allowed * 4);
            break;
        }

        default:
            break;
    }
}
