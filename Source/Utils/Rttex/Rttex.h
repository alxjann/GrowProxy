#pragma once

#include "Core/Types.h"

#include <vector>

struct RgbaImage {
    int width = 0;
    int height = 0;
    std::vector<uint8> pixels; // width * height * 4 bytes, top row first
};

// Decodes a Growtopia .rttex texture, optionally wrapped in a zlib RTPACK, into RGBA pixels.
// Only uncompressed 8-bit RGB/RGBA textures are supported (the format of the game's tile sheets).
bool DecodeRttex(const std::vector<uint8>& file, RgbaImage& out);
