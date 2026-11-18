#pragma once

#include "Core/Types.h"

#include <vector>

namespace Zlib {
    // Decompresses zlib data whose uncompressed size is known.
    bool Inflate(const uint8* data, size_t size, size_t uncompressedSize, std::vector<uint8>& out);
}
