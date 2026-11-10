#include "Game/Player/Inventory/Inventory.h"

#include "Utils/ByteReader.h"

#include <algorithm>

bool Inventory::Load(const uint8* data, size_t size) {
    ByteReader r(data, size);
    r.Skip(1); // version
    uint32 slots = r.Read<uint32>();
    uint16 count = r.Read<uint16>();

    std::vector<InventoryItem> items(count);
    for (InventoryItem& item : items) {
        r.Read(item.itemID);
        r.Read(item.count);
        r.Read(item.flags);
    }

    if (!r.Ok())
        return false;

    m_slots = slots;
    m_items = std::move(items);
    return true;
}

void Inventory::Clear() {
    m_slots = 0;
    m_items.clear();
}

void Inventory::SetCount(uint16 itemID, uint8 count) {
    auto it = std::find_if(m_items.begin(), m_items.end(),
                           [itemID](const InventoryItem& item) { return item.itemID == itemID; });

    if (count == 0) {
        if (it != m_items.end())
            m_items.erase(it);
    } else if (it != m_items.end()) {
        it->count = count;
    } else {
        m_items.push_back({itemID, count, 0});
    }
}

uint8 Inventory::GetCount(uint16 itemID) const {
    for (const InventoryItem& item : m_items) {
        if (item.itemID == itemID)
            return item.count;
    }
    return 0;
}
