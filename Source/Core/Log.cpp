#include "Core/Log.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <deque>
#include <mutex>

namespace {

    constexpr size_t MAX_HISTORY = 4000;

    std::mutex s_mutex;
    std::deque<Log::Entry> s_history;
    uint64 s_sequence = 0;

    void Write(Log::Level level, const char* fmt, va_list args) {
        static const char* const COLORS[] = {"\x1b[32m", "\x1b[33m", "\x1b[31m"};
        static const char* const TAGS[] = {"INFO ", "WARN ", "ERROR"};

        char message[4096];
        vsnprintf(message, sizeof(message), fmt, args);

        std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_s(&local, &now);

        std::lock_guard<std::mutex> lock(s_mutex);
        std::printf("\x1b[90m%02d:%02d:%02d\x1b[0m %s%s\x1b[0m %s\n", local.tm_hour, local.tm_min, local.tm_sec,
                    COLORS[(int)level], TAGS[(int)level], message);
        std::fflush(stdout);

        Log::Entry entry;
        entry.sequence = ++s_sequence;
        entry.hour = local.tm_hour;
        entry.minute = local.tm_min;
        entry.second = local.tm_sec;
        entry.level = level;
        entry.message = message;
        s_history.push_back(std::move(entry));
        if (s_history.size() > MAX_HISTORY)
            s_history.pop_front();
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

uint64 Log::Fetch(uint64 afterSequence, std::vector<Entry>& out) {
    std::lock_guard<std::mutex> lock(s_mutex);
    for (const Entry& entry : s_history) {
        if (entry.sequence > afterSequence)
            out.push_back(entry);
    }
    return s_sequence;
}

#define LOG_IMPL(level)                                                                                                \
    va_list args;                                                                                                      \
    va_start(args, fmt);                                                                                               \
    Write(level, fmt, args);                                                                                           \
    va_end(args);

void Log::Info(const char* fmt, ...) {
    LOG_IMPL(Level::Info);
}

void Log::Warn(const char* fmt, ...) {
    LOG_IMPL(Level::Warn);
}

void Log::Error(const char* fmt, ...) {
    LOG_IMPL(Level::Error);
}
