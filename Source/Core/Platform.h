#pragma once

#include "Core/Types.h"

namespace Platform {
    // Directory containing the running executable (UTF-8, no trailing slash).
    string GetExecutableDir();

    std::wstring ToWide(string_view utf8);

    // Milliseconds from a monotonic clock.
    uint64 NowMs();

    // True while a key or mouse button is held down, regardless of which window has focus.
    bool IsKeyDown(int virtualKey);
}
