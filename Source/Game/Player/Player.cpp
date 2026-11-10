#include "Game/Player/Player.h"

GameUpdatePacket LocalPlayer::SetLongPunch(bool enabled) {
    longPunchActive = enabled;

    GameUpdatePacket packet;
    packet.type = PACKET_SET_CHARACTER_STATE;
    packet.netID = netID;
    packet.pos = {1000.0f, 400.0f};
    packet.speed = {250.0f, 1200.0f};
    packet.byte2 = enabled ? 255 : 128; // punch range: 255 = unlimited, 128 = default
    packet.byte3 = enabled ? 255 : 128; // build range
    return packet;
}
