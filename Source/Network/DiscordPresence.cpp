#include "Network/DiscordPresence.h"

#include "Core/Config.h"
#include "Core/Log.h"

#include <windows.h>

namespace {

    constexpr uint32 RETRY_MS = 15000; // how often to look for Discord while it is not running
    constexpr uint32 POLL_MS = 1000;   // how often to check that Discord is still connected

    // Discord IPC opcodes. Each message is: uint32 opcode, uint32 length, JSON.
    constexpr uint32 HANDSHAKE = 0;
    constexpr uint32 FRAME = 1;
    constexpr uint32 CLOSE = 2;

    HANDLE OpenPipe() {
        // Discord listens on the first free pipe of discord-ipc-0..9
        for (int i = 0; i < 10; ++i) {
            string name = "\\\\.\\pipe\\discord-ipc-" + std::to_string(i);
            HANDLE pipe = CreateFileA(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (pipe != INVALID_HANDLE_VALUE)
                return pipe;
        }
        return INVALID_HANDLE_VALUE;
    }

    bool Write(HANDLE pipe, uint32 opcode, const string& json) {
        uint32 header[2] = {opcode, (uint32)json.size()};
        string message(reinterpret_cast<const char*>(header), sizeof(header));
        message += json;

        DWORD written = 0;
        return WriteFile(pipe, message.data(), (DWORD)message.size(), &written, nullptr) && written == message.size();
    }

    // Blocks until Discord sends a message.
    bool Read(HANDLE pipe, uint32& opcode, string& json) {
        uint32 header[2];
        DWORD read = 0;
        if (!ReadFile(pipe, header, sizeof(header), &read, nullptr) || read != sizeof(header))
            return false;

        opcode = header[0];
        json.resize(header[1]);
        return header[1] == 0 || (ReadFile(pipe, json.data(), header[1], &read, nullptr) && read == header[1]);
    }

    bool IsOpen(HANDLE pipe) {
        return PeekNamedPipe(pipe, nullptr, 0, nullptr, nullptr, nullptr) != 0;
    }

    string ActivityJson() {
        return R"({"cmd":"SET_ACTIVITY","nonce":"1","args":{"pid":)" + std::to_string(GetCurrentProcessId()) +
               R"(,"activity":{"assets":{"large_image":")" + Config::DISCORD_IMAGE_KEY +
               R"(","large_text":"GrowProxy"},"buttons":[{"label":"Website","url":")" + Config::WEBSITE_URL +
               R"("}]}}})";
    }

    // Says hello to Discord and sets the presence. Discord replies to each message, with CLOSE or an ERROR
    // event when something is wrong (such as an unknown application ID).
    bool SetPresence(HANDLE pipe) {
        uint32 opcode = 0;
        string reply;

        string handshake = R"({"v":1,"client_id":")" + string(Config::DISCORD_APP_ID) + R"("})";
        if (!Write(pipe, HANDSHAKE, handshake) || !Read(pipe, opcode, reply))
            return false;
        if (opcode == CLOSE) {
            Log::Warn("[Discord] Discord refused the connection: %s", reply.c_str());
            return false;
        }

        if (!Write(pipe, FRAME, ActivityJson()) || !Read(pipe, opcode, reply))
            return false;
        if (reply.find(R"("evt":"ERROR")") != string::npos) {
            Log::Warn("[Discord] Discord rejected the presence: %s", reply.c_str());
            return false;
        }
        return true;
    }

}

DiscordPresence::~DiscordPresence() {
    Stop();
}

void DiscordPresence::Start() {
    m_stopping = false;
    m_thread = std::thread(&DiscordPresence::Run, this);
}

void DiscordPresence::Stop() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopping = true;
    }
    m_wake.notify_all();
    if (m_thread.joinable())
        m_thread.join();
}

bool DiscordPresence::WaitFor(uint32 ms) {
    std::unique_lock<std::mutex> lock(m_mutex);
    return !m_wake.wait_for(lock, std::chrono::milliseconds(ms), [this] { return m_stopping; });
}

void DiscordPresence::Run() {
    do {
        HANDLE pipe = OpenPipe();
        if (pipe == INVALID_HANDLE_VALUE)
            continue; // Discord is not running

        if (SetPresence(pipe)) {
            Log::Info("[Discord] Rich Presence is on");

            // the presence lasts while the pipe stays open
            while (WaitFor(POLL_MS) && IsOpen(pipe)) {
            }
            Log::Info("[Discord] Rich Presence is off");
        }

        CloseHandle(pipe);
    } while (WaitFor(RETRY_MS));
}
