#include "Game/Items/Items.h"

#include "Core/Log.h"
#include "Utils/ByteReader.h"

namespace {

    constexpr uint16 MAX_SUPPORTED_VERSION = 27;

    // From version 3 item names are XOR-ed with this key, starting at (id % key length).
    string DecodeName(const string& encoded, int32 id) {
        constexpr string_view key = "PBG892FXX982ABC*";

        string name = encoded;
        size_t k = (size_t)id % key.size();
        for (char& c : name) {
            c ^= key[k];
            k = (k + 1) % key.size();
        }
        return name;
    }

    // Reads one item. Fields the proxy does not use are skipped, but every field must be consumed.
    void ReadItem(ByteReader& r, uint16 version, Item& item) {
        r.Read(item.id);
        r.Read(item.flags);
        r.Read(item.type);
        r.Read(item.material);

        item.name = r.ReadString();
        if (version >= 3)
            item.name = DecodeName(item.name, item.id);

        r.ReadString(item.texture);
        r.Skip(4 + 1 + 4); // texture hash, visual effect, cooking time
        r.Read(item.textureX);
        r.Read(item.textureY);
        r.Skip(1 + 1); // storage type, layer
        r.Read(item.collisionType);
        r.Read(item.breakHits);
        r.Skip(4 + 1); // restore time, body part
        r.Read(item.rarity);
        r.Read(item.maxAmount);
        r.SkipString(); // extra string
        r.Skip(4 + 4);  // extra string hash, animation ms

        if (version > 3) {
            r.SkipString(); // pet name
            r.SkipString(); // pet prefix
            r.SkipString(); // pet suffix
        }
        if (version > 4)
            r.SkipString(); // pet ability

        r.Skip(4);     // seed base, seed overlay, tree base, tree leaves
        r.Skip(4 + 4); // seed colors
        r.Skip(2 + 2); // unused
        r.Read(item.growTime);

        if (version > 6) {
            r.Skip(4);      // fx flags
            r.SkipString(); // animation textures
        }
        if (version > 7) {
            r.SkipString(); // overlay texture
            r.SkipString(); // second animation textures
            r.Skip(8);      // dual animation layer
        }
        if (version > 8)
            r.Skip(4 + 60); // flags2, client data
        if (version > 9)
            r.Skip(4 + 4); // tile range, pile size
        if (version > 10)
            r.SkipString(); // punch parameters
        if (version > 11)
            r.Skip(4 + 9); // extra slots
        if (version > 12)
            r.Skip(4); // light source range
        if (version > 13)
            r.Skip(4); // variant item
        if (version > 14) {
            r.Skip(1 + 8 + 8 + 8); // chair: enabled, player offset, arm position, arm offset
            r.SkipString();        // chair arm texture
        }
        if (version > 15)
            r.SkipString(); // config name
        if (version > 16)
            r.Skip(4); // hit particle
        if (version > 17)
            r.Skip(4); // config name hash
        if (version > 18)
            r.Skip(1 + 4 + 4); // random sprite: enabled, offset, chance
        if (version > 19)
            r.Skip(1); // hidden body parts
        if (version > 20)
            r.Skip(1); // can transform
        if (version > 21)
            r.ReadString(item.description);
        if (version > 22)
            r.Skip(2 + 2); // recipe seeds
        if (version > 23)
            r.Skip(1); // slippery type
        if (version > 24) {
            r.SkipString();
            r.Skip(4);
        }
        if (version > 25)
            r.Skip(1);
        if (version > 26)
            r.SkipString();
    }

}

bool ItemDatabase::Load(const uint8* data, size_t size) {
    ByteReader reader(data, size);
    uint16 version = reader.Read<uint16>();
    uint32 count = reader.Read<uint32>();

    if (!reader.Ok() || version > MAX_SUPPORTED_VERSION) {
        Log::Error("[Items] Unsupported items.dat version %u", version);
        return false;
    }

    std::vector<Item> items(count);
    for (Item& item : items) {
        ReadItem(reader, version, item);
        if (!reader.Ok()) {
            Log::Error("[Items] items.dat is truncated at item %d", item.id);
            return false;
        }
    }

    m_items = std::move(items);
    m_version = version;
    return true;
}

const Item* ItemDatabase::Get(int32 id) const {
    return id >= 0 && (size_t)id < m_items.size() ? &m_items[id] : nullptr;
}

string ItemDatabase::GetName(int32 id) const {
    const Item* item = Get(id);
    return item && !item->name.empty() ? item->name : "Item " + std::to_string(id);
}
