#include "Utils/Rttex/Rttex.h"

#include "Utils/ByteReader.h"
#include "Utils/Zlib/Zlib.h"

#include <cstring>

namespace {

    constexpr size_t RTPACK_HEADER = 32;
    constexpr size_t RTTEX_HEADER = 124; // texture header (100) + first mip level header (24)
    constexpr int GL_UNSIGNED_BYTE = 5121;
    constexpr int MAX_SIZE = 8192;
    constexpr uint32 MAX_UNPACKED = 64 * 1024 * 1024;

}

// RTPACK: "RTPACK", uint8 version, uint8, uint32 compressed size, uint32 size, uint8 compression (1 = zlib), 15 bytes
// RTTXTR: "RTTXTR", uint8 version, uint8, int32 height, int32 width, int32 format, int32 original height,
//         int32 original width, uint8 has alpha, uint8 already compressed, 2 bytes, int32 mip count, 64 bytes,
//         then per mip level: int32 height, int32 width, int32 size, int32 level, 8 bytes, followed by the pixels
//         of the first (largest) level, bottom row first.
bool DecodeRttex(const std::vector<uint8>& file, RgbaImage& out) {
    const uint8* data = file.data();
    size_t size = file.size();

    std::vector<uint8> unpacked;
    if (size >= RTPACK_HEADER && std::memcmp(data, "RTPACK", 6) == 0) {
        ByteReader r(data, size);
        r.Skip(8);
        uint32 packedSize = r.Read<uint32>();
        uint32 unpackedSize = r.Read<uint32>();
        uint8 compression = r.Read<uint8>();
        if (!r.Ok() || compression != 1 || packedSize > size - RTPACK_HEADER || unpackedSize > MAX_UNPACKED)
            return false;
        if (!Zlib::Inflate(data + RTPACK_HEADER, packedSize, unpackedSize, unpacked))
            return false;
        data = unpacked.data();
        size = unpacked.size();
    }

    if (size < RTTEX_HEADER || std::memcmp(data, "RTTXTR", 6) != 0)
        return false;

    ByteReader r(data, size);
    r.Skip(8);
    int32 height = r.Read<int32>();
    int32 width = r.Read<int32>();
    int32 format = r.Read<int32>();
    r.Skip(8); // original height and width
    uint8 hasAlpha = r.Read<uint8>();
    uint8 compressed = r.Read<uint8>();
    if (!r.Ok() || format != GL_UNSIGNED_BYTE || compressed || width <= 0 || height <= 0 || width > MAX_SIZE ||
        height > MAX_SIZE)
        return false;

    const int bytesPerPixel = hasAlpha ? 4 : 3;
    const size_t rowSize = (size_t)width * bytesPerPixel;
    if (size - RTTEX_HEADER < rowSize * height)
        return false;

    out.width = width;
    out.height = height;
    out.pixels.resize((size_t)width * height * 4);
    for (int y = 0; y < height; ++y) {
        const uint8* src = data + RTTEX_HEADER + (size_t)(height - 1 - y) * rowSize; // stored bottom row first
        uint8* dst = out.pixels.data() + (size_t)y * width * 4;
        for (int x = 0; x < width; ++x, src += bytesPerPixel, dst += 4) {
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = hasAlpha ? src[3] : 255;
        }
    }
    return true;
}
