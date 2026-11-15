#pragma once

#include "Core/Types.h"

#include <vector>

// Thread-safe logging with a timestamp and a level tag. printf-style formatting.
// Messages go to the console (when there is one) and to an in-memory history that the UI reads.
namespace Log {
    enum class Level : uint8 { Info, Warn, Error };

    struct Entry {
        uint64 sequence = 0; // increases by one per message, starting at 1
        int32 hour = 0;
        int32 minute = 0;
        int32 second = 0;
        Level level = Level::Info;
        string message;
    };

    void Init();

    void Info(const char* fmt, ...);
    void Warn(const char* fmt, ...);
    void Error(const char* fmt, ...);

    // Appends the messages newer than afterSequence to out (oldest first). Only the most recent messages are
    // kept, so entries may be skipped. Returns the sequence of the newest message.
    uint64 Fetch(uint64 afterSequence, std::vector<Entry>& out);
}
