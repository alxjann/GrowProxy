#pragma once

#include "Proxy/Proxy.h"

#include <deque>
#include <vector>

struct CapturedPacket {
    uint64 number = 0;   // 1, 2, ... in capture order
    double timeSec = 0;  // seconds since the capture started
    char clock[16] = {}; // "hh:mm:ss.mmm" local time
    Target to = Server;
    bool fromProxy = false;
    bool blocked = false;
    MessageType messageType = MESSAGE_UNKNOWN;
    int32 gameType = -1; // GamePacketType for game packets, -1 otherwise
    string summary;      // function name, text action or packet type
    std::vector<uint8> data;

    PacketView View() const { return {data.data(), data.size()}; }
};

// Keeps the most recent packets reported by Proxy::SetPacketObserver for the packet inspector.
// Not thread-safe: guarded by the same lock as the proxy.
class PacketCapture {
public:
    void Add(const PacketEvent& event);
    void Clear();

    const std::deque<CapturedPacket>& Packets() const { return m_packets; }

    bool recording = true;
    size_t capacity = 5000;

private:
    std::deque<CapturedPacket> m_packets;
    uint64 m_nextNumber = 1;
    double m_startTime = -1.0;
};

namespace PacketNames {
    const char* Message(MessageType type);
    const char* Game(int32 type);
}
