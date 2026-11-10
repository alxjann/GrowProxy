#pragma once

// Thread-safe console logging with a timestamp and a colored level tag. printf-style formatting.
namespace Log {
    void Init();

    void Info(const char* fmt, ...);
    void Warn(const char* fmt, ...);
    void Error(const char* fmt, ...);
}
