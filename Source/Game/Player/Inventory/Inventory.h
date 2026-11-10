#pragma once

#include "Core/Types.h"

#include <vector>

struct InventoryItem {
    uint16 itemID = 0;
    uint8 count = 0;
    uint8 flags = 0; // bit 0: equipped

    bool IsEquipped() const { return (flags & 1) != 0; }
};

// The local player's backpack (SEND_INVENTORY_STATE / MODIFY_ITEM_INVENTORY).
class Inventory {
public:
    // uint8 version, uint32 slots, uint16 count, then 4 bytes per item.
    bool Load(const uint8* data, size_t size);
    void Clear();

    // Sets the amount of an item; 0 removes it.
    void SetCount(uint16 itemID, uint8 count);
    uint8 GetCount(uint16 itemID) const;

    uint32 GetSlots() const { return m_slots; }
    const std::vector<InventoryItem>& GetItems() const { return m_items; }

private:
    uint32 m_slots = 0;
    std::vector<InventoryItem> m_items;
};
