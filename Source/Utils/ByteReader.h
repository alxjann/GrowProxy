#pragma once

#include "Core/Types.h"

#include <cstring>
#include <type_traits>

class ByteReader {
public:
    ByteReader(const void* data, size_t size) : m_data((const uint8*)data), m_size(size) {}

    template <typename T> bool Read(T& out) {
        static_assert(std::is_trivially_copyable_v<T>, "Read<T> needs a plain data type");
        if (!Has(sizeof(T)))
            return false;
        std::memcpy(&out, m_data + m_pos, sizeof(T));
        m_pos += sizeof(T);
        return true;
    }

    template <typename T> T Read() {
        T value{};
        Read(value);
        return value;
    }

    bool ReadBytes(void* out, size_t size) {
        if (!Has(size))
            return false;
        std::memcpy(out, m_data + m_pos, size);
        m_pos += size;
        return true;
    }

    // Strings are prefixed with a uint16 length.
    bool ReadString(string& out) {
        uint16 length = 0;
        if (!Read(length) || !Has(length))
            return false;
        out.assign((const char*)m_data + m_pos, length);
        m_pos += length;
        return true;
    }

    string ReadString() {
        string value;
        ReadString(value);
        return value;
    }

    bool Skip(size_t size) {
        if (!Has(size))
            return false;
        m_pos += size;
        return true;
    }

    bool SkipString() { return Skip(Read<uint16>()); }

    // Next byte without consuming it, 0 at the end.
    uint8 Peek() const { return m_pos < m_size ? m_data[m_pos] : 0; }

    bool Ok() const { return m_ok; }
    size_t Position() const { return m_pos; }
    size_t Size() const { return m_size; }
    size_t Remaining() const { return m_size - m_pos; }
    const uint8* Current() const { return m_data + m_pos; }

private:
    bool Has(size_t size) {
        if (!m_ok || size > m_size - m_pos) {
            m_ok = false;
            return false;
        }
        return true;
    }

    const uint8* m_data;
    size_t m_size;
    size_t m_pos = 0;
    bool m_ok = true;
};
