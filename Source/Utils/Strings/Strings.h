#pragma once

#include "Core/Types.h"

#include <optional>

namespace Strings {
    string_view Trim(string_view text);
    string ToLower(string_view text);
    bool StartsWith(string_view text, string_view prefix);

    // Removes Growtopia color codes (` followed by one character).
    string StripColorCodes(string_view text);

    std::optional<int32> ToInt(string_view text);
    std::optional<float> ToFloat(string_view text);
}
