#include "Packets/Packets.h"

#include <cstring>

MessageType PacketView::Type() const {
    uint32 type = MESSAGE_UNKNOWN;
    if (size >= 4)
        std::memcpy(&type, data, 4);
    return (MessageType)type;
}

string_view PacketView::Text() const {
    if (size <= 4)
        return {};

    string_view text((const char*)data + 4, size - 4);
    while (!text.empty() && text.back() == '\0')
        text.remove_suffix(1);
    return text;
}

bool PacketView::GetGamePacket(GameUpdatePacket& out, const uint8** extraData) const {
    if (size < 4 + sizeof(GameUpdatePacket))
        return false;

    std::memcpy(&out, data + 4, sizeof(GameUpdatePacket));

    if (!(out.flags & PACKET_FLAG_EXTENDED_DATA))
        out.extraDataSize = 0;
    else if (out.extraDataSize > size - 4 - sizeof(GameUpdatePacket))
        return false;

    if (extraData)
        *extraData = out.extraDataSize > 0 ? data + 4 + sizeof(GameUpdatePacket) : nullptr;
    return true;
}

std::vector<uint8> MakeTextPacket(MessageType type, string_view text) {
    std::vector<uint8> packet(4 + text.size() + 1, 0);
    std::memcpy(packet.data(), &type, 4);
    std::memcpy(packet.data() + 4, text.data(), text.size());
    return packet;
}

std::vector<uint8> MakeGamePacket(GameUpdatePacket packet, const void* extraData, uint32 extraSize) {
    if (extraData && extraSize > 0) {
        packet.flags |= PACKET_FLAG_EXTENDED_DATA;
        packet.extraDataSize = extraSize;
    } else {
        extraSize = 0;
        packet.extraDataSize = 0;
    }

    MessageType type = MESSAGE_GAME_PACKET;
    std::vector<uint8> out(4 + sizeof(GameUpdatePacket) + extraSize);
    std::memcpy(out.data(), &type, 4);
    std::memcpy(out.data() + 4, &packet, sizeof(GameUpdatePacket));
    if (extraSize > 0)
        std::memcpy(out.data() + 4 + sizeof(GameUpdatePacket), extraData, extraSize);
    return out;
}
