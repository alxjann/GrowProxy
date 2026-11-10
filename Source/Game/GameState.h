#pragma once

#include "Game/Items/Items.h"
#include "Game/Player/Inventory/Inventory.h"
#include "Game/Player/Player.h"
#include "Game/World/World.h"

#include <unordered_map>

// Everything the proxy knows about the game session. Only touched from the network thread.
struct GameState {
    ItemDatabase items;
    World world;
    Inventory inventory;
    LocalPlayer local;
    std::unordered_map<int32, Player> players; // by netID, includes the local player

    bool antigravityEnabled = false;

    void ClearPlayers() {
        players.clear();
        local.netID = -1;
        local.userID = -1;
        local.name.clear();
    }

    // Leaving a world (or quitting) invalidates everything world related.
    void LeaveWorld(bool clearInventory) {
        world.Clear();
        ClearPlayers();
        if (clearInventory)
            inventory.Clear();
    }
};
