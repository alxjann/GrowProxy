#include "Core/Platform.h"

#include <windows.h>

#include <chrono>

string Platform::GetExecutableDir() {
    wchar_t path[MAX_PATH] = {};
    DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);

    std::wstring dir(path, length);
    dir = dir.substr(0, dir.find_last_of(L"\\/"));

    int size = WideCharToMultiByte(CP_UTF8, 0, dir.c_str(), (int)dir.size(), nullptr, 0, nullptr, nullptr);
    string utf8(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, dir.c_str(), (int)dir.size(), utf8.data(), size, nullptr, nullptr);
    return utf8;
}

std::wstring Platform::ToWide(string_view utf8) {
    if (utf8.empty())
        return L"";

    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), nullptr, 0);
    std::wstring wide(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), wide.data(), size);
    return wide;
}

uint64 Platform::NowMs() {
    using namespace std::chrono;
    return (uint64)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

bool Platform::IsKeyDown(int virtualKey) {
    return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}
