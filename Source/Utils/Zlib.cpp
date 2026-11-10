#include "Utils/Zlib.h"

#include <zlib.h>

bool Zlib::Inflate(const uint8* data, size_t size, size_t uncompressedSize, std::vector<uint8>& out) {
    out.resize(uncompressedSize);

    uLongf outSize = (uLongf)uncompressedSize;
    int result = uncompress(out.data(), &outSize, data, (uLong)size);
    if (result != Z_OK || outSize != uncompressedSize) {
        out.clear();
        return false;
    }
    return true;
}
