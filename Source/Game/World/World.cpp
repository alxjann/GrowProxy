#include "Game/World/World.h"

#include "Core/Log.h"
#include "Game/Items/Items.h"
#include "Game/World/TileExtras.h"
#include "Utils/ByteReader.h"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace {

    constexpr int MAX_TILES = 255 * 255;

    bool IsAreaLock(uint16 itemID) {
        switch (itemID) {
            case 202:  // Small Lock
            case 204:  // Big Lock
            case 206:  // Huge Lock
            case 4994: // Builder's Lock
                return true;
            default:
                return false;
        }
    }

    // Tiles of these items store an extra CBOR blob (uint32 size + bytes) after the tile.
    bool HasCborData(uint16 itemID) {
        switch (itemID) {
            case 3548:  // Battle Pet Cage
            case 8624:  // Bountiful Flowering Lattice Roots
            case 8630:  // Bountiful Climbing Hydrangea Lattice Roots
            case 8636:  // Bountiful Flowering Garland Roots
            case 8642:  // Bountiful Lattice Fence Roots
            case 8648:  // Bountiful Jungle Temple Roots
            case 8654:  // Bountiful Jungle Temple Background Roots
            case 8660:  // Bountiful Jungle Temple Door Roots
            case 8666:  // Bountiful Jungle Temple Pillar Roots
            case 8672:  // Bountiful Bamboo Background Roots
            case 8678:  // Bountiful Bamboo Platform Roots
            case 8684:  // Bountiful Bamboo Ladder Roots
            case 8690:  // Bountiful Bamboo Spikes Roots
            case 8696:  // Bountiful White Doll's Eyes Roots
            case 8702:  // Bountiful Monkshood Roots
            case 8708:  // Bountiful Corpse Flower Roots
            case 8714:  // Bountiful Growtopian-Eating Looming Plant Roots
            case 14662: // Operating Table
            case 14666: // Auto Surgeon Station
            case 15376: // Party Projector
            case 15546: // Auction Block
            case 16094: // Weather Machine - Concert Lights
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

    if (!r.Ok() || m_width <= 0 || m_height <= 0 || tileCount != m_width * m_height ||
        tileCount > MAX_TILES) {
        Log::Error("[World] Invalid world header (%dx%d, %u tiles)", m_width, m_height, tileCount);
        Clear();
        return false;
    }

    r.Skip(4 + 1);

    m_tiles.resize(tileCount);
    for (uint32 i = 0; i < tileCount; ++i) {
        TileAccess access;
        bool hasAccess = false;
        if (ReadTile(r, m_tiles[i], items, access, hasAccess)) {
            StoreAccess(i, m_tiles[i], std::move(access), hasAccess, items);
        } else {
            Log::Error("[World] Failed to read tile %u (%u, %u) of %s", i, i % m_width, i / m_width, m_name.c_str());

            // The tile that fails is rarely the broken one: an extra read with the wrong size a few tiles earlier
            // shifts everything after it. Log the last extras and keep the raw data so the layout can be fixed.

/*
            for (uint32 j = i, shown = 0; j-- > 0 && shown < 12;) {
                if (m_tiles[j].extraType != TILE_EXTRA_NONE && ++shown)
                    Log::Warn("[World]   earlier extra: tile %u item %u extra type %u", j, m_tiles[j].foreground,
                              m_tiles[j].extraType);
            }

            if (std::ofstream dump("world_dump.bin", std::ios::binary); dump) {
                dump.write((const char*)data, (std::streamsize)size);
                Log::Warn("[World] Raw world data saved to world_dump.bin");
            }
*/
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
    m_access.clear();
    m_worldLock = -1;
    m_lastDroppedUid = 0;
    m_baseWeather = m_weather = 0;
}

bool World::ReadTile(ByteReader& r, Tile& tile, const ItemDatabase& items, TileAccess& access, bool& hasAccess) {
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
        hasAccess = ReadTileExtra(r, type, tile, m_version, access);
    }

    if (HasCborData(tile.foreground))
        r.Skip(r.Read<uint32>());

    return r.Ok();
}

bool World::UpdateTile(int x, int y, const uint8* data, size_t size, const ItemDatabase& items) {
    Tile* tile = GetTile(x, y);
    if (!tile)
        return false;

    ByteReader r(data, size);
    Tile updated;
    TileAccess access;
    bool hasAccess = false;
    if (!ReadTile(r, updated, items, access, hasAccess))
        return false;

    int index = y * m_width + x;
    if (tile->extraType == TILE_EXTRA_LOCK && updated.extraType != TILE_EXTRA_LOCK)
        ReleaseLockArea(index);

    // an update may leave out the protecting lock; keep the one we know
    if (!updated.HasFlag(TILE_FLAG_HAS_PARENT) && tile->HasFlag(TILE_FLAG_HAS_PARENT)) {
        updated.flags |= TILE_FLAG_HAS_PARENT;
        updated.parent = tile->parent;
    }

    *tile = updated;
    StoreAccess(index, updated, std::move(access), hasAccess, items);
    return true;
}

void World::StoreAccess(int index, const Tile& tile, TileAccess&& access, bool hasAccess, const ItemDatabase& items) {
    if (m_worldLock == index)
        m_worldLock = -1;

    if (!hasAccess) {
        m_access.erase(index);
        return;
    }

    m_access[index] = std::move(access);
    const Item* item = items.Get(tile.foreground);
    if (tile.extraType == TILE_EXTRA_LOCK && item && item->type == ITEM_TYPE_LOCK && !IsAreaLock(tile.foreground))
        m_worldLock = index;
}

void World::ReleaseLockArea(int lockIndex) {
    for (Tile& tile : m_tiles) {
        if (tile.HasFlag(TILE_FLAG_HAS_PARENT) && tile.parent == lockIndex) {
            tile.flags &= ~TILE_FLAG_HAS_PARENT;
            tile.parent = 0;
        }
    }
}

void World::ClearForeground(int x, int y) {
    Tile* tile = GetTile(x, y);
    if (!tile)
        return;

    int index = y * m_width + x;
    if (tile->extraType == TILE_EXTRA_LOCK)
        ReleaseLockArea(index);

    tile->foreground = 0;
    tile->fruits = 0;
    tile->plantedAt = 0;
    tile->flags &= ~TILE_FLAG_HAS_EXTRA_DATA;
    tile->extraType = TILE_EXTRA_NONE;
    m_access.erase(index);
    if (m_worldLock == index)
        m_worldLock = -1;
}

void World::SetLockArea(int x, int y, const uint8* indices, size_t size) {
    if (!GetTile(x, y))
        return;

    int lockIndex = y * m_width + x;
    ReleaseLockArea(lockIndex);

    for (size_t i = 0; i + 2 <= size; i += 2) {
        uint16 index;
        std::memcpy(&index, indices + i, 2);
        if (index >= m_tiles.size() || index == lockIndex)
            continue;
        m_tiles[index].flags |= TILE_FLAG_HAS_PARENT;
        m_tiles[index].parent = lockIndex;
    }
}

const TileAccess* World::GetAccess(int x, int y) const {
    if (!GetTile(x, y))
        return nullptr;
    auto it = m_access.find(y * m_width + x);
    return it != m_access.end() ? &it->second : nullptr;
}

bool World::HasAccess(int x, int y, int userID) const {
    const Tile* tile = GetTile(x, y);
    if (!tile)
        return false;

    int lockIndex = tile->HasFlag(TILE_FLAG_HAS_PARENT) ? tile->parent : m_worldLock;
    if (lockIndex < 0)
        return true;

    auto lock = m_access.find(lockIndex);
    if (lock == m_access.end())
        return false;

    if (m_tiles[lockIndex].HasFlag(TILE_FLAG_IS_OPEN_TO_PUBLIC) || lock->second.Allows(userID))
        return true;

    auto worldLock = m_worldLock >= 0 ? m_access.find(m_worldLock) : m_access.end();
    return worldLock != m_access.end() && userID > 0 && worldLock->second.owner == userID;
}

Tile* World::GetTile(int x, int y) {
    if (!m_loaded || x < 0 || y < 0 || x >= m_width || y >= m_height)
        return nullptr;
    return &m_tiles[y * m_width + x];
}

void World::AddDroppedItem(int itemID, int count, Vec2f pos, int flags) {
    DroppedItem item;
    item.uid = ++m_lastDroppedUid;
    item.itemID = itemID;
    item.count = count;
    item.flags = flags;
    item.pos = pos;
    m_droppedItems.push_back(item);
}

void World::ModifyDroppedItem(int uid, int itemID, int count, Vec2f pos, int flags) {
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

void World::RemoveDroppedItem(int uid) {
    auto it = std::find_if(m_droppedItems.begin(), m_droppedItems.end(), [uid](const DroppedItem& item) { return item.uid == uid; });
    if (it != m_droppedItems.end()) {
        *it = m_droppedItems.back();
        m_droppedItems.pop_back();
    }
}
