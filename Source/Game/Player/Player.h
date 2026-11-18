#pragma once

#include "Core/Types.h"
#include "Packets/Packets/Packets.h"

struct Player {
    int netID = -1;
    int userID = -1;
    string name; // without color codes
    Vec2i tile;  // last known tile position
};

struct LocalPlayer {
    int netID = -1;
    int userID = -1;
    string name;
    Vec2f pos; // pixels, from the client's own state packets

    bool longPunchActive = false;

    // Turns long punch on or off; returns the character state packet to send to the client.
    GameUpdatePacket SetLongPunch(bool enabled);
};
