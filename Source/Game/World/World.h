#pragma once

#include "Core/Types.h"

#include <vector>

class ByteReader;
class ItemDatabase;

enum TileFlag : uint16 {
    TILE_FLAG_HAS_EXTRA_DATA = 1 << 0,
    TILE_FLAG_HAS_PARENT = 1 << 1,
    TILE_FLAG_WAS_SPLICED = 1 << 2,
    TILE_FLAG_WILL_SPAWN_SEEDS = 1 << 3,
    TILE_FLAG_IS_SEEDLING = 1 << 4,
    TILE_FLAG_FLIPPED_X = 1 << 5,
    TILE_FLAG_IS_ON = 1 << 6,
    TILE_FLAG_IS_OPEN_TO_PUBLIC = 1 << 7,
    TILE_FLAG_BG_IS_ON = 1 << 8,
    TILE_FLAG_FG_ALT_MODE = 1 << 9,
    TILE_FLAG_IS_WET = 1 << 10,
    TILE_FLAG_GLUED = 1 << 11,
    TILE_FLAG_ON_FIRE = 1 << 12,
    TILE_FLAG_PAINTED_RED = 1 << 13,
    TILE_FLAG_PAINTED_GREEN = 1 << 14,
    TILE_FLAG_PAINTED_BLUE = 1 << 15,
};

struct Tile {
    uint16 foreground = 0;
    uint16 background = 0;
    uint16 parent = 0;
    uint16 flags = 0;
    uint8 extraType = 0; // TileExtraType, 0 if none

    bool HasFlag(uint16 flag) const { return (flags & flag) != 0; }
};

struct DroppedItem {
    uint32 uid = 0;
    uint16 itemID = 0;
    uint8 count = 0;
    uint8 flags = 0;
    Vec2f pos;
};

// The world the player is currently in, as sent by the server (SEND_MAP_DATA).
class World {
public:
    bool Load(const uint8* data, size_t size, const ItemDatabase& items);
    void Clear();

    // Replaces one tile from a SEND_TILE_UPDATE_DATA payload.
    bool UpdateTile(int32 x, int32 y, const uint8* data, size_t size, const ItemDatabase& items);

    bool IsLoaded() const { return m_loaded; }
    const string& GetName() const { return m_name; }
    uint16 GetVersion() const { return m_version; }
    int32 GetWidth() const { return m_width; }
    int32 GetHeight() const { return m_height; }
    uint16 GetWeather() const { return m_weather; }
    uint16 GetBaseWeather() const { return m_baseWeather; }

    Tile* GetTile(int32 x, int32 y);
    const Tile* GetTile(int32 x, int32 y) const { return const_cast<World*>(this)->GetTile(x, y); }
    std::vector<Tile>& GetTiles() { return m_tiles; }
    const std::vector<Tile>& GetTiles() const { return m_tiles; }

    std::vector<DroppedItem>& GetDroppedItems() { return m_droppedItems; }
    void AddDroppedItem(uint16 itemID, uint8 count, Vec2f pos, uint8 flags);
    void ModifyDroppedItem(uint32 uid, uint16 itemID, uint8 count, Vec2f pos, uint8 flags);
    void RemoveDroppedItem(uint32 uid);

private:
    bool ReadTile(ByteReader& reader, Tile& tile, const ItemDatabase& items);

    bool m_loaded = false;
    uint16 m_version = 0;
    string m_name;
    int32 m_width = 0;
    int32 m_height = 0;
    std::vector<Tile> m_tiles;
    std::vector<DroppedItem> m_droppedItems;
    uint32 m_lastDroppedUid = 0;
    uint16 m_baseWeather = 0;
    uint16 m_weather = 0;
};
