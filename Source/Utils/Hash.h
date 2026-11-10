#pragma once

#include "Core/Types.h"

// FNV-1a, usable at compile time so strings can be used in switch statements: switch (Hash(s)) { case "x"_hash: }
constexpr uint32 Hash(string_view text) {
    uint32 hash = 2166136261u;
    for (char c : text) {
        hash ^= (uint8)c;
        hash *= 16777619u;
    }
    return hash;
}

constexpr uint32 operator""_hash(const char* text, size_t length) {
    return Hash(string_view(text, length));
}
