#include "Core/Log.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <mutex>

namespace {

    std::mutex s_mutex;

    void Write(const char* color, const char* tag, const char* fmt, va_list args) {
        char message[4096];
        vsnprintf(message, sizeof(message), fmt, args);

        std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_s(&local, &now);

        std::lock_guard<std::mutex> lock(s_mutex);
        std::printf("\x1b[90m%02d:%02d:%02d\x1b[0m %s%s\x1b[0m %s\n", local.tm_hour, local.tm_min, local.tm_sec, color,
                    tag, message);
        std::fflush(stdout);
    }

}

void Log::Init() {
    // enable ANSI colors in the Windows console
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(console, &mode))
        SetConsoleMode(console, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    SetConsoleOutputCP(CP_UTF8);
}

#define LOG_IMPL(color, tag)                                                                                           \
    va_list args;                                                                                                      \
    va_start(args, fmt);                                                                                               \
    Write(color, tag, fmt, args);                                                                                      \
    va_end(args);

void Log::Info(const char* fmt, ...) {
    LOG_IMPL("\x1b[32m", "INFO ");
}

void Log::Warn(const char* fmt, ...) {
    LOG_IMPL("\x1b[33m", "WARN ");
}

void Log::Error(const char* fmt, ...) {
    LOG_IMPL("\x1b[31m", "ERROR");
}
