#pragma once

#include "Core/Types.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

// Shows GrowProxy on the user's Discord profile (Rich Presence): the logo, the app name, and a button to the
// website. Talks to the Discord desktop app over its local named pipe on a background thread, and
// reconnects when Discord is started or restarted later. Does nothing when Discord is not running.
class DiscordPresence {
public:
    DiscordPresence() = default;
    ~DiscordPresence();

    DiscordPresence(const DiscordPresence&) = delete;
    DiscordPresence& operator=(const DiscordPresence&) = delete;

    void Start();
    void Stop();

private:
    void Run();
    bool WaitFor(uint32 ms); // false once Stop was called

    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_wake;
    bool m_stopping = false;
};
