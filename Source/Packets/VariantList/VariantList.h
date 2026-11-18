#pragma once

#include "Core/Types.h"

#include <initializer_list>
#include <variant>
#include <vector>

// Argument list of a call-function packet (e.g. {"OnConsoleMessage", "hello"}).
using Variant = std::variant<std::monostate, float, string, Vec2f, Vec3f, uint32, int32>;

class VariantList {
public:
    VariantList() = default;
    VariantList(std::initializer_list<Variant> values) : m_values(values) {}

    // Parses the call-function payload. Fails on unknown types or truncated data.
    bool Parse(const uint8* data, size_t size);
    std::vector<uint8> Serialize() const;

    size_t Size() const { return m_values.size(); }
    Variant& operator[](size_t index) { return m_values[index]; }
    const Variant& operator[](size_t index) const { return m_values[index]; }

    // Typed access; returns an empty/zero value for a missing index or another type.
    const string& GetString(size_t index) const;
    int32 GetInt(size_t index) const; // accepts int32 and uint32
    uint32 GetUInt(size_t index) const;

private:
    std::vector<Variant> m_values;
};
