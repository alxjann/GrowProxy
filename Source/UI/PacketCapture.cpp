#include "UI/PacketCapture.h"

#include "Core/Platform.h"
#include "Packets/TextPacket.h"

#include <windows.h>

namespace {

    // Short description shown in the packet list.
    string Summarize(const PacketView& view, MessageType type, int32 gameType) {
        if (type == MESSAGE_GENERIC_TEXT || type == MESSAGE_GAME_MESSAGE || type == MESSAGE_TRACK) {
            string_view text = view.Text();
            TextPacket packet(text);
            if (packet.Has("action"))
                return "action|" + string(packet.Get("action"));

            string_view line = text.substr(0, text.find('\n'));
            if (line.size() > 64)
                return string(line.substr(0, 64)) + "...";
            return string(line);
        }

        if (type != MESSAGE_GAME_PACKET || gameType < 0)
            return PacketNames::Message(type);

        GameUpdatePacket game;
        const uint8* extra = nullptr;
        if (!view.GetGamePacket(game, &extra))
            return PacketNames::Game(gameType);

        if (game.type == PACKET_CALL_FUNCTION && extra) {
            VariantList call;
            if (call.Parse(extra, game.extraDataSize) && !call.GetString(0).empty())
                return call.GetString(0);
        }

        char text[96];
        switch (game.type) {
            case PACKET_STATE:
            case PACKET_TILE_CHANGE_REQUEST:
            case PACKET_TILE_APPLY_DAMAGE:
            case PACKET_TILE_ACTIVATE_REQUEST:
                snprintf(text, sizeof(text), "%s  (%d, %d)", PacketNames::Game(game.type), game.tileX, game.tileY);
                return text;
            default:
                return PacketNames::Game(game.type);
        }
    }

}

void PacketCapture::Add(const PacketEvent& event) {
    if (!recording || !event.data || event.size == 0)
        return;

    double now = (double)Platform::NowMs() / 1000.0;
    if (m_startTime < 0.0)
        m_startTime = now;

    CapturedPacket packet;
    packet.number = m_nextNumber++;
    packet.timeSec = now - m_startTime;
    packet.to = event.to;
    packet.fromProxy = event.fromProxy;
    packet.blocked = event.blocked;
    packet.data.assign(event.data, event.data + event.size);

    SYSTEMTIME local;
    GetLocalTime(&local);
    snprintf(packet.clock, sizeof(packet.clock), "%02u:%02u:%02u.%03u", local.wHour, local.wMinute, local.wSecond,
             local.wMilliseconds);

    PacketView view = packet.View();
    packet.messageType = view.Type();
    if (packet.messageType == MESSAGE_GAME_PACKET && view.size >= 5)
        packet.gameType = view.data[4];
    packet.summary = Summarize(view, packet.messageType, packet.gameType);

    m_packets.push_back(std::move(packet));
    while (m_packets.size() > capacity)
        m_packets.pop_front();
}

void PacketCapture::Clear() {
    m_packets.clear();
}

const char* PacketNames::Message(MessageType type) {
    switch (type) {
        case MESSAGE_SERVER_HELLO:
            return "Server hello";
        case MESSAGE_GENERIC_TEXT:
            return "Generic text";
        case MESSAGE_GAME_MESSAGE:
            return "Game message";
        case MESSAGE_GAME_PACKET:
            return "Game packet";
        case MESSAGE_ERROR:
            return "Error";
        case MESSAGE_TRACK:
            return "Track";
        case MESSAGE_CLIENT_LOG_REQUEST:
            return "Client log request";
        case MESSAGE_CLIENT_LOG_RESPONSE:
            return "Client log response";
        default:
            return "Unknown";
    }
}

const char* PacketNames::Game(int32 type) {
    static const char* const NAMES[] = {
        "State",
        "Call function",
        "Update status",
        "Tile change request",
        "Send map data",
        "Send tile update data",
        "Send tile update data (multiple)",
        "Tile activate request",
        "Tile apply damage",
        "Send inventory state",
        "Item activate request",
        "Item activate object request",
        "Send tile tree state",
        "Modify item inventory",
        "Item change object",
        "Send lock",
        "Send item database data",
        "Send particle effect",
        "Set icon state",
        "Item effect",
        "Set character state",
        "Ping reply",
        "Ping request",
        "Got punched",
        "App check response",
        "App integrity fail",
        "Disconnect",
        "Battle join",
        "Battle event",
        "Use door",
        "Send parental",
        "Gone fishin'",
        "Steam",
        "Pet battle",
        "NPC",
        "Special",
        "Send particle effect v2",
        "Active arrow to item",
        "Select tile index",
        "Send player tribute data",
        "On step tile mod",
    };
    if (type >= 0 && type < (int32)(sizeof(NAMES) / sizeof(NAMES[0])))
        return NAMES[type];
    return "Unknown game packet";
}
