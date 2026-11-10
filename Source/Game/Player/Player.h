#pragma once

#include "Core/Types.h"
#include "Packets/Packets.h"

struct Player {
    int32 netID = -1;
    int32 userID = -1;
    string name; // without color codes
    Vec2i tile;  // last known tile position
};

struct LocalPlayer {
    int32 netID = -1;
    int32 userID = -1;
    string name;
    Vec2f pos; // pixels, from the client's own state packets

    bool longPunchActive = false;

    // Turns long punch on or off; returns the character state packet to send to the client.
    GameUpdatePacket SetLongPunch(bool enabled);
};
