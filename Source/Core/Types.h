#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

using int8 = std::int8_t;
using int16 = std::int16_t;
using int32 = std::int32_t;
using int64 = std::int64_t;
using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;

using string = std::string;
using std::string_view;

struct Vec2f {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3f {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec2i {
    int32 x = 0;
    int32 y = 0;
};
