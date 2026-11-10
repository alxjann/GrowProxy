#include "Game/World/World.h"

#include "Core/Log.h"
#include "Game/Items/Items.h"
#include "Game/World/TileExtras.h"
#include "Utils/ByteReader.h"

#include <algorithm>

namespace {

    constexpr int32 MAX_TILES = 255 * 255;

    // Tiles of these items store an extra CBOR blob (uint32 size + bytes) after the tile.
    bool HasCborData(uint16 itemID) {
        switch (itemID) {
            case 3548:
            case 8624:
            case 8630:
            case 8636:
            case 8642:
            case 8648:
            case 8654:
            case 8660:
            case 8666:
            case 8672:
            case 8678:
            case 8684:
            case 8690:
            case 8696:
            case 8702:
            case 8708:
            case 8714:
            case 14662:
            case 14666:
            case 15376:
            case 15546:
                return true;
            default:
                return false;
        }
    }

}

// Layout (client format):
//   uint16 version, uint32 flags, string name
//   int32 width, int32 height, uint32 tileCount, uint32 + uint8 (unknown), tiles
//   3 x uint32 (unknown)
//   uint32 droppedCount, uint32 lastDroppedUid, dropped items (16 bytes each)
//   uint16 baseWeather, uint16, uint16 weather, uint16, uint32
bool World::Load(const uint8* data, size_t size, const ItemDatabase& items) {
    Clear();
    ByteReader r(data, size);

    r.Read(m_version);
    r.Skip(4); // flags
    r.ReadString(m_name);
    r.Read(m_width);
    r.Read(m_height);
    uint32 tileCount = r.Read<uint32>();

    if (!r.Ok() || m_width <= 0 || m_height <= 0 || tileCount != (uint32)(m_width * m_height) ||
        tileCount > MAX_TILES) {
        Log::Error("[World] Invalid world header (%dx%d, %u tiles)", m_width, m_height, tileCount);
        Clear();
        return false;
    }

    r.Skip(4 + 1);

    m_tiles.resize(tileCount);
    for (uint32 i = 0; i < tileCount; ++i) {
        if (!ReadTile(r, m_tiles[i], items)) {
            Log::Error("[World] Failed to read tile %u (%u, %u) of %s", i, i % m_width, i / m_width, m_name.c_str());
            Clear();
            return false;
        }
    }

    m_loaded = true;

    // Everything after the tiles is best effort: a damaged tail loses the dropped items or weather, not the world.
    r.Skip(3 * 4);

    uint32 droppedCount = r.Read<uint32>();
    r.Read(m_lastDroppedUid);
    if (r.Ok() && droppedCount <= r.Remaining() / 16) {
        m_droppedItems.resize(droppedCount);
        for (DroppedItem& item : m_droppedItems) {
            r.Read(item.itemID);
            r.Read(item.pos);
            r.Read(item.count);
            r.Read(item.flags);
            r.Read(item.uid);
        }
    }

    r.Read(m_baseWeather);
    r.Skip(2);
    r.Read(m_weather);
    r.Skip(2 + 4);

    if (!r.Ok())
        Log::Warn("[World] %s ends early; dropped items or weather may be missing", m_name.c_str());
    else if (r.Remaining() > 0)
        Log::Warn("[World] %zu unread bytes at the end of %s", r.Remaining(), m_name.c_str());

    return true;
}

void World::Clear() {
    m_loaded = false;
    m_version = 0;
    m_name.clear();
    m_width = m_height = 0;
    m_tiles.clear();
    m_droppedItems.clear();
    m_lastDroppedUid = 0;
    m_baseWeather = m_weather = 0;
}

bool World::ReadTile(ByteReader& r, Tile& tile, const ItemDatabase& items) {
    r.Read(tile.foreground);
    r.Read(tile.background);
    r.Read(tile.parent);
    r.Read(tile.flags);
    tile.extraType = TILE_EXTRA_NONE;

    if (tile.HasFlag(TILE_FLAG_HAS_PARENT))
        r.Read(tile.parent); // stored a second time

    if (tile.HasFlag(TILE_FLAG_HAS_EXTRA_DATA)) {
        // the extra starts with its type; older data may have 0 there, then the item decides
        uint8 type = r.Peek();
        if (type == TILE_EXTRA_NONE) {
            const Item* item = items.Get(tile.foreground);
            type = item ? GetTileExtraTypeForItem(item->id, item->type) : TILE_EXTRA_NONE;
        }
        if (type == TILE_EXTRA_NONE) {
            Log::Error("[World] Unknown tile extra for item %u", tile.foreground);
            return false;
        }

        tile.extraType = type;
        SkipTileExtra(r, type, tile.foreground, m_version);
    }

    if (HasCborData(tile.foreground))
        r.Skip(r.Read<uint32>());

    return r.Ok();
}

bool World::UpdateTile(int32 x, int32 y, const uint8* data, size_t size, const ItemDatabase& items) {
    Tile* tile = GetTile(x, y);
    if (!tile)
        return false;

    ByteReader r(data, size);
    Tile updated;
    if (!ReadTile(r, updated, items))
        return false;

    *tile = updated;
    return true;
}

Tile* World::GetTile(int32 x, int32 y) {
    if (!m_loaded || x < 0 || y < 0 || x >= m_width || y >= m_height)
        return nullptr;
    return &m_tiles[(size_t)y * m_width + x];
}

void World::AddDroppedItem(uint16 itemID, uint8 count, Vec2f pos, uint8 flags) {
    m_droppedItems.push_back({++m_lastDroppedUid, itemID, count, flags, pos});
}

void World::ModifyDroppedItem(uint32 uid, uint16 itemID, uint8 count, Vec2f pos, uint8 flags) {
    for (DroppedItem& item : m_droppedItems) {
        if (item.uid == uid) {
            item.itemID = itemID;
            item.count = count;
            item.pos = pos;
            item.flags = flags;
            return;
        }
    }
}

void World::RemoveDroppedItem(uint32 uid) {
    auto it = std::find_if(m_droppedItems.begin(), m_droppedItems.end(),
                           [uid](const DroppedItem& item) { return item.uid == uid; });
    if (it != m_droppedItems.end()) {
        *it = m_droppedItems.back();
        m_droppedItems.pop_back();
    }
}
